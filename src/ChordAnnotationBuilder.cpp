#include "ChordAnnotationBuilder.h"

#include <algorithm>
#include <array>
#include <limits>
#include <QStringList>

namespace {
constexpr int PitchClasses = 12;
constexpr int CandidateLimit = 6;
constexpr int MaxSegmentsPerBarLimit = 4;
constexpr double EmptyScore = -0.15;
constexpr double SplitPenalty = 0.08;
constexpr double SplitGain = 0.04;
constexpr double MinSegmentWeightRatio = 0.18;

struct ChordQuality
{
    const char *suffix;
    std::array<int, 5> tones;
    int toneCount;
    std::array<int, 4> required;
    int requiredCount;
};

struct BarFeatures
{
    std::array<double, PitchClasses> weight;
    std::array<double, PitchClasses> onsetWeight;
    int bassPitchClass = -1;
    double totalWeight = 0.0;
};

struct ChordCandidate
{
    int root = -1;
    int bass = -1;
    QString suffix;
    double score = EmptyScore;
    float confidence = 0.0f;
};

struct ChordSegment
{
    int barIndex = -1;
    qint64 startTick = 0;
    qint64 endTick = 0;
    BarFeatures features;
    QVector<ChordCandidate> candidates;
    double pathScore = -std::numeric_limits<double>::infinity();
};

struct SegmentPlan
{
    QVector<ChordSegment> segments;
    double score = -std::numeric_limits<double>::infinity();
};

const ChordQuality qualities[] = {
    {"maj7", {{0, 4, 7, 11, -1}}, 4, {{0, 4, 11, -1}}, 3},
    {"m7", {{0, 3, 7, 10, -1}}, 4, {{0, 3, 10, -1}}, 3},
    {"7", {{0, 4, 7, 10, -1}}, 4, {{0, 4, 10, -1}}, 3},
    {"m7b5", {{0, 3, 6, 10, -1}}, 4, {{0, 3, 6, 10}}, 4},
    {"dim7", {{0, 3, 6, 9, -1}}, 4, {{0, 3, 6, 9}}, 4},
    {"6", {{0, 4, 7, 9, -1}}, 4, {{0, 4, 9, -1}}, 3},
    {"m6", {{0, 3, 7, 9, -1}}, 4, {{0, 3, 9, -1}}, 3},
    {"", {{0, 4, 7, -1, -1}}, 3, {{0, 4, -1, -1}}, 2},
    {"m", {{0, 3, 7, -1, -1}}, 3, {{0, 3, -1, -1}}, 2},
    {"dim", {{0, 3, 6, -1, -1}}, 3, {{0, 3, 6, -1}}, 3},
    {"aug", {{0, 4, 8, -1, -1}}, 3, {{0, 4, 8, -1}}, 3},
    {"sus4", {{0, 5, 7, -1, -1}}, 3, {{0, 5, -1, -1}}, 2},
    {"sus2", {{0, 2, 7, -1, -1}}, 3, {{0, 2, -1, -1}}, 2}
};

bool containsTone(const ChordQuality& quality, int interval)
{
    for (int i = 0; i < quality.toneCount; i++)
        if (quality.tones[i] == interval)
            return true;
    return false;
}

bool validSource(const NoteEvent& note, const ChordAnnotationOptions& options)
{
    if (note.channel == MIDI_DRUM_CHANNEL || note.pitch < 0 || note.pitch >= MAX_MIDI_NOTES)
        return false;
    if (options.sourceChannel >= 0 && note.channel != options.sourceChannel)
        return false;
    return options.sourceTrack < 0 || note.track == options.sourceTrack;
}

qint64 barEndTick(const BarMap& bars, int bar)
{
    if (bar + 1 < bars.barStarts.size())
        return bars.barStarts[bar + 1];
    return bars.durationTicks > bars.barStarts[bar] ? bars.durationTicks : bars.barStarts[bar];
}

int firstKeySignature(const SongData& song, int fallback)
{
    if (fallback != ChordAnnotationAutoKeySignature)
        return fallback;
    for (const MidiEventRecord& record : song.events)
        if (record.event.type() == MIDI_PB_keySignature)
            return record.event.data1();
    return 0;
}

BarFeatures emptyFeatures()
{
    BarFeatures features;
    features.weight.fill(0.0);
    features.onsetWeight.fill(0.0);
    return features;
}

double noteWeight(const NoteEvent& note, qint64 overlap, bool startsNearDownbeat)
{
    const double velocity = note.velocity > 0 ? static_cast<double>(note.velocity) / 127.0 : 0.55;
    const double onset = startsNearDownbeat ? 1.35 : 1.0;
    return static_cast<double>(overlap) * (0.45 + velocity) * onset;
}

void addNote(BarFeatures& features, const NoteEvent& note, qint64 start, qint64 end,
             qint64 beatLength, int& lowestDownbeat, int& lowestBar)
{
    const qint64 overlapStart = std::max(start, note.startTick);
    const qint64 overlapEnd = std::min(end, note.endTick);
    if (overlapEnd <= overlapStart)
        return;
    const bool downbeat = note.startTick <= start + beatLength / 2 && note.endTick > start;
    const int pc = normalizedPitchClass(note.pitch);
    const double weight = noteWeight(note, overlapEnd - overlapStart, downbeat);
    features.weight[pc] += weight;
    features.totalWeight += weight;
    if (downbeat)
        features.onsetWeight[pc] += weight;
    if (downbeat && (lowestDownbeat < 0 || note.pitch < lowestDownbeat))
        lowestDownbeat = note.pitch;
    if (lowestBar < 0 || note.pitch < lowestBar)
        lowestBar = note.pitch;
}

QVector<BarFeatures> extractFeatures(const SongData& song, const BarMap& bars,
                                      const ChordAnnotationOptions& options)
{
    QVector<BarFeatures> features(bars.barStarts.size());
    QVector<int> lowestDownbeat(bars.barStarts.size(), -1);
    QVector<int> lowestBar(bars.barStarts.size(), -1);
    for (BarFeatures& bar : features)
        bar = emptyFeatures();
    for (const NoteEvent& note : song.notes)
    {
        if (!validSource(note, options))
            continue;
        const int firstBar = barAtTick(bars, note.startTick);
        const int lastBar = barAtTick(bars, std::max(note.startTick, note.endTick - 1));
        for (int bar = firstBar; bar <= lastBar && bar < features.size(); bar++)
            addNote(features[bar], note, bars.barStarts[bar], barEndTick(bars, bar),
                    bars.beatLengths.value(bar, song.ppqn), lowestDownbeat[bar], lowestBar[bar]);
    }
    for (int bar = 0; bar < features.size(); bar++)
    {
        const int bass = lowestDownbeat[bar] >= 0 ? lowestDownbeat[bar] : lowestBar[bar];
        features[bar].bassPitchClass = bass >= 0 ? normalizedPitchClass(bass) : -1;
    }
    return features;
}

BarFeatures extractFeaturesInRange(const SongData& song, const ChordAnnotationOptions& options,
                                   qint64 start, qint64 end, qint64 beatLength)
{
    BarFeatures features = emptyFeatures();
    int lowestDownbeat = -1;
    int lowestBar = -1;
    if (end <= start)
        return features;
    for (const NoteEvent& note : song.notes)
    {
        if (validSource(note, options))
            addNote(features, note, start, end, beatLength, lowestDownbeat, lowestBar);
    }
    const int bass = lowestDownbeat >= 0 ? lowestDownbeat : lowestBar;
    features.bassPitchClass = bass >= 0 ? normalizedPitchClass(bass) : -1;
    return features;
}

double toneValue(int interval)
{
    if (interval == 0)
        return 1.80;
    if (interval == 3 || interval == 4 || interval == 10 || interval == 11)
        return 1.45;
    if (interval == 7)
        return 0.55;
    return 1.05;
}

double scoreQuality(const BarFeatures& features, int root, const ChordQuality& quality)
{
    double score = 0.0;
    for (int pc = 0; pc < PitchClasses; pc++)
    {
        const int interval = normalizedPitchClass(pc - root);
        score += features.weight[pc] * (containsTone(quality, interval) ? toneValue(interval) : -0.35);
    }
    for (int i = 0; i < quality.requiredCount; i++)
    {
        const int pc = normalizedPitchClass(root + quality.required[i]);
        if (features.weight[pc] <= 0.0)
            score -= std::max(10.0, features.totalWeight * 0.20);
    }
    if (features.bassPitchClass == root)
        score += features.totalWeight * 0.18;
    const double strong = std::max(16.0, features.totalWeight * 0.10);
    if (quality.suffix[0] == 'm' && features.weight[normalizedPitchClass(root + 4)] >= strong &&
            features.weight[normalizedPitchClass(root + 10)] >= strong)
        score -= features.totalWeight * 0.35;
    if (quality.suffix[0] == '7' && quality.suffix[1] == '\0' &&
            features.weight[normalizedPitchClass(root + 3)] >= strong &&
            features.weight[normalizedPitchClass(root + 4)] >= strong)
        score += features.totalWeight * 0.18;
    score += features.onsetWeight[root] * 0.30;
    return score / std::max(1.0, features.totalWeight);
}

bool strongTone(const BarFeatures& features, int root, int interval)
{
    const int pc = normalizedPitchClass(root + interval);
    return features.weight[pc] >= std::max(16.0, features.totalWeight * 0.10);
}

QString decoratedSuffix(const BarFeatures& features, int root, QString suffix)
{
    QStringList extensions;
    const bool dominant = suffix == QStringLiteral("7");
    if (dominant && strongTone(features, root, 1))
        extensions.append(QStringLiteral("b9"));
    if (strongTone(features, root, 2))
        extensions.append(QStringLiteral("9"));
    if (dominant && strongTone(features, root, 3))
        extensions.append(QStringLiteral("#9"));
    if (strongTone(features, root, 5) && !suffix.contains(QStringLiteral("sus")))
        extensions.append(QStringLiteral("11"));
    if (dominant && strongTone(features, root, 6))
        extensions.append(QStringLiteral("#11"));
    if (dominant && strongTone(features, root, 8))
        extensions.append(QStringLiteral("b13"));
    if (strongTone(features, root, 9) && !suffix.contains(QStringLiteral("6")))
        extensions.append(QStringLiteral("13"));
    for (int i = 0; i < extensions.size(); i++)
        suffix += (i == 0) ? extensions[i] : QStringLiteral("(") + extensions[i] + QStringLiteral(")");
    return suffix;
}

QString labelSuffix(const BarFeatures& features, int root, const ChordQuality& quality,
                    ChordAnnotationDetail detail)
{
    QString suffix = QString::fromLatin1(quality.suffix);
    if (detail == ChordAnnotationBasic)
    {
        if (suffix == QStringLiteral("maj7") || suffix == QStringLiteral("6") ||
                suffix == QStringLiteral("7"))
            return QString();
        if (suffix == QStringLiteral("m7") || suffix == QStringLiteral("m6"))
            return QStringLiteral("m");
        if (suffix == QStringLiteral("m7b5") || suffix == QStringLiteral("dim7"))
            return QStringLiteral("dim");
        return suffix;
    }
    if (detail == ChordAnnotationSevenths)
        return suffix;
    return decoratedSuffix(features, root, suffix);
}

ChordCandidate makeCandidate(const BarFeatures& features, int root,
                             const ChordQuality& quality, ChordAnnotationDetail detail)
{
    ChordCandidate candidate;
    candidate.root = root;
    candidate.bass = features.bassPitchClass;
    candidate.suffix = labelSuffix(features, root, quality, detail);
    candidate.score = scoreQuality(features, root, quality);
    return candidate;
}

QVector<ChordCandidate> candidatesForBar(const BarFeatures& features,
                                         const ChordAnnotationOptions& options)
{
    QVector<ChordCandidate> result;
    if (features.totalWeight <= 0.0)
    {
        result.append(ChordCandidate());
        return result;
    }
    for (int root = 0; root < PitchClasses; root++)
        for (const ChordQuality& quality : qualities)
            result.append(makeCandidate(features, root, quality, options.detail));
    std::sort(result.begin(), result.end(), [](const ChordCandidate& a, const ChordCandidate& b) {
        return a.score > b.score;
    });
    const double second = result.size() > 1 ? result[1].score : EmptyScore;
    const float confidence = static_cast<float>(std::max(0.0, result[0].score - second));
    result[0].confidence = confidence;
    if (confidence < options.minConfidence &&
            options.lowConfidenceMode == ChordAnnotationHideLowConfidence)
    {
        result.clear();
        result.append(ChordCandidate());
        return result;
    }
    if (confidence < options.minConfidence &&
            options.lowConfidenceMode == ChordAnnotationShowConservative)
        result[0].suffix.clear();
    result.resize(std::min(CandidateLimit, result.size()));
    result.append(ChordCandidate());
    return result;
}

int maxSegmentsPerBar(const ChordAnnotationOptions& options)
{
    if (!options.intraBarSegmentation)
        return 1;
    return std::max(1, std::min(MaxSegmentsPerBarLimit, options.maxSegmentsPerBar));
}

QVector<qint64> beatBoundaries(const SongData& song, const BarMap& bars, int bar)
{
    QVector<qint64> boundaries;
    const qint64 start = bars.barStarts[bar];
    const qint64 end = barEndTick(bars, bar);
    qint64 beat = bars.beatLengths.value(bar, song.ppqn);
    if (beat <= 0)
        beat = song.ppqn > 0 ? song.ppqn : end - start;
    boundaries.append(start);
    for (qint64 tick = start + beat; beat > 0 && tick < end; tick += beat)
        boundaries.append(tick);
    if (boundaries.last() != end)
        boundaries.append(end);
    return boundaries;
}

ChordSegment makeSegment(int bar, qint64 start, qint64 end, const BarFeatures& features,
                         const ChordAnnotationOptions& options, qint64 barLength)
{
    ChordSegment segment;
    segment.barIndex = bar;
    segment.startTick = start;
    segment.endTick = end;
    segment.features = features;
    segment.candidates = candidatesForBar(features, options);
    const double length = static_cast<double>(std::max<qint64>(1, end - start));
    segment.pathScore = segment.candidates.first().score * length / std::max<qint64>(1, barLength);
    return segment;
}

bool sameHarmony(const ChordCandidate& a, const ChordCandidate& b)
{
    return a.root == b.root && a.bass == b.bass && a.suffix == b.suffix;
}

bool validSplitSegment(const ChordSegment& segment, double minWeight)
{
    return segment.candidates.first().root >= 0 && segment.features.totalWeight >= minWeight;
}

bool splitSupported(const ChordSegment& left, const ChordSegment& right, double minWeight)
{
    if (!validSplitSegment(left, minWeight) || !validSplitSegment(right, minWeight))
        return false;
    const ChordCandidate& chord = right.candidates.first();
    if (sameHarmony(left.candidates.first(), chord))
        return false;
    const double strong = std::max(16.0, right.features.totalWeight * 0.08);
    return right.features.onsetWeight[chord.root] >= strong ||
            (right.features.bassPitchClass >= 0 &&
             right.features.bassPitchClass != left.features.bassPitchClass);
}

bool canAppendSegment(const QVector<ChordSegment>& segments, int count,
                      const QVector<QVector<int>>& parent, int k, int start,
                      int end, double minWeight)
{
    const ChordSegment& segment = segments[start * count + end];
    if (k == 1)
        return start == 0;
    if (parent[k - 1][start] < 0)
        return false;
    const ChordSegment& previous = segments[parent[k - 1][start] * count + start];
    return splitSupported(previous, segment, minWeight);
}

QVector<ChordSegment> reconstructSegments(const QVector<ChordSegment>& segments, int count,
                                          const QVector<QVector<int>>& parent, int segmentCount)
{
    QVector<ChordSegment> result;
    int end = count - 1;
    for (int k = segmentCount; k > 0; k--)
    {
        const int start = parent[k][end];
        if (start < 0)
            return QVector<ChordSegment>();
        result.prepend(segments[start * count + end]);
        end = start;
    }
    return result;
}

SegmentPlan bestSegmentPlan(const QVector<ChordSegment>& segments, int count,
                            int maxSegments, double wholeScore, double minWeight)
{
    QVector<QVector<double>> dp(maxSegments + 1, QVector<double>(count, -std::numeric_limits<double>::infinity()));
    QVector<QVector<int>> parent(maxSegments + 1, QVector<int>(count, -1));
    dp[0][0] = 0.0;
    for (int k = 1; k <= maxSegments; k++)
        for (int end = 1; end < count; end++)
            for (int start = 0; start < end; start++)
            {
                if (dp[k - 1][start] < -1.0e100 ||
                        !canAppendSegment(segments, count, parent, k, start, end, minWeight))
                    continue;
                const double score = dp[k - 1][start] + segments[start * count + end].pathScore -
                        (k > 1 ? SplitPenalty : 0.0);
                if (score > dp[k][end])
                {
                    dp[k][end] = score;
                    parent[k][end] = start;
                }
            }
    SegmentPlan plan;
    for (int k = 2; k <= maxSegments; k++)
        if (dp[k][count - 1] > wholeScore + SplitGain && dp[k][count - 1] > plan.score)
        {
            plan.score = dp[k][count - 1];
            plan.segments = reconstructSegments(segments, count, parent, k);
        }
    return plan;
}

QVector<ChordSegment> segmentsForBar(const SongData& song, const BarMap& bars,
                                     const ChordAnnotationOptions& options, int bar,
                                     const BarFeatures& wholeFeatures)
{
    const qint64 start = bars.barStarts[bar];
    const qint64 end = barEndTick(bars, bar);
    const qint64 barLength = std::max<qint64>(1, end - start);
    QVector<ChordSegment> whole;
    whole.append(makeSegment(bar, start, end, wholeFeatures, options, barLength));
    const QVector<qint64> boundaries = beatBoundaries(song, bars, bar);
    const int count = boundaries.size();
    const int limit = std::min(maxSegmentsPerBar(options), count - 1);
    if (limit <= 1 || count < 3)
        return whole;
    QVector<ChordSegment> intervals(count * count);
    for (int i = 0; i < count - 1; i++)
        for (int j = i + 1; j < count; j++)
        {
            const bool isWhole = i == 0 && j == count - 1;
            const BarFeatures features = isWhole ? wholeFeatures :
                        extractFeaturesInRange(song, options, boundaries[i], boundaries[j],
                                               bars.beatLengths.value(bar, song.ppqn));
            intervals[i * count + j] = makeSegment(bar, boundaries[i], boundaries[j], features, options, barLength);
        }
    const double minWeight = wholeFeatures.totalWeight * MinSegmentWeightRatio;
    const SegmentPlan plan = bestSegmentPlan(intervals, count, limit,
                                             whole.first().candidates.first().score, minWeight);
    return plan.segments.isEmpty() ? whole : plan.segments;
}

double transitionScore(const ChordCandidate& from, const ChordCandidate& to)
{
    if (from.root < 0 || to.root < 0)
        return -0.03;
    if (from.root == to.root && from.suffix == to.suffix)
        return 0.16;
    if (from.root == to.root)
        return 0.06;
    const int movement = normalizedPitchClass(to.root - from.root);
    return (movement == 5 || movement == 7) ? 0.04 : -0.04;
}

QVector<int> bestPath(const QVector<QVector<ChordCandidate>>& bars)
{
    QVector<QVector<double>> dp(bars.size());
    QVector<QVector<int>> parent(bars.size());
    for (int i = 0; i < bars.size(); i++)
    {
        dp[i].fill(-std::numeric_limits<double>::infinity(), bars[i].size());
        parent[i].fill(-1, bars[i].size());
        for (int j = 0; j < bars[i].size(); j++)
            for (int k = 0; k < (i == 0 ? 1 : bars[i - 1].size()); k++)
            {
                const double previous = i == 0 ? 0.0 : dp[i - 1][k] + transitionScore(bars[i - 1][k], bars[i][j]);
                if (previous + bars[i][j].score > dp[i][j])
                {
                    dp[i][j] = previous + bars[i][j].score;
                    parent[i][j] = k;
                }
            }
    }
    QVector<int> path(bars.size());
    path.last() = static_cast<int>(std::max_element(dp.last().begin(), dp.last().end()) - dp.last().begin());
    for (int i = bars.size() - 1; i > 0; i--)
        path[i - 1] = parent[i][path[i]];
    return path;
}

ChordAnnotation annotationFor(const ChordCandidate& candidate, int bar,
                              qint64 startTick, qint64 endTick, int keySignature)
{
    ChordAnnotation annotation;
    annotation.barIndex = bar;
    annotation.startTick = startTick;
    annotation.endTick = endTick;
    annotation.rootPitchClass = candidate.root;
    annotation.bassPitchClass = candidate.bass;
    annotation.suffix = candidate.suffix;
    annotation.confidence = candidate.confidence;
    annotation.label = formatChordSymbol(candidate.root, candidate.suffix, candidate.bass, keySignature);
    return annotation;
}

void applyCarryPolicy(QVector<ChordAnnotation>& annotations, bool enabled)
{
    if (!enabled)
        return;
    QString previousLabel;
    QString previousSuffix;
    int previousRoot = -1;
    int previousBass = -1;
    for (ChordAnnotation& annotation : annotations)
    {
        if (!annotation.label.isEmpty())
        {
            previousLabel = annotation.label;
            previousSuffix = annotation.suffix;
            previousRoot = annotation.rootPitchClass;
            previousBass = annotation.bassPitchClass;
        }
        else if (!previousLabel.isEmpty())
        {
            annotation.label = previousLabel;
            annotation.suffix = previousSuffix;
            annotation.rootPitchClass = previousRoot;
            annotation.bassPitchClass = previousBass;
            annotation.confidence = 0.25f;
        }
    }
}

void mergeAdjacentSameHarmony(QVector<ChordAnnotation>& annotations)
{
    for (int i = 1; i < annotations.size();)
    {
        ChordAnnotation& previous = annotations[i - 1];
        const ChordAnnotation& current = annotations[i];
        const bool same = previous.barIndex == current.barIndex &&
                previous.rootPitchClass == current.rootPitchClass &&
                previous.bassPitchClass == current.bassPitchClass &&
                previous.suffix == current.suffix;
        if (!same)
        {
            i++;
            continue;
        }
        previous.endTick = current.endTick;
        annotations.removeAt(i);
    }
}

QVector<ChordAnnotation> buildNaiveChordAnnotations(const SongData& song, const BarMap& bars,
                                                    ChordAnnotationOptions options)
{
    QVector<ChordAnnotation> annotations;
    if (bars.barStarts.isEmpty())
        return annotations;
    const int keySignature = firstKeySignature(song, options.keySignature);
    const QVector<BarFeatures> features = extractFeatures(song, bars, options);
    QVector<ChordSegment> segments;
    QVector<QVector<ChordCandidate>> candidates;
    for (int bar = 0; bar < bars.barStarts.size(); bar++)
    {
        if (barEndTick(bars, bar) <= bars.barStarts[bar])
            continue;
        const QVector<ChordSegment> barSegments = segmentsForBar(song, bars, options, bar, features[bar]);
        for (const ChordSegment& segment : barSegments)
        {
            segments.append(segment);
            candidates.append(segment.candidates);
        }
    }
    if (candidates.isEmpty())
        return annotations;
    const QVector<int> path = options.useSmoothing ? bestPath(candidates) : QVector<int>();
    for (int i = 0; i < candidates.size(); i++)
    {
        const int index = options.useSmoothing ? path[i] : 0;
        annotations.append(annotationFor(candidates[i][index], segments[i].barIndex,
                                         segments[i].startTick, segments[i].endTick, keySignature));
    }
    applyCarryPolicy(annotations, options.carryEmptyBars);
    mergeAdjacentSameHarmony(annotations);
    return annotations;
}
}

QVector<ChordAnnotation> buildChordAnnotations(const SongData& song, const BarMap& bars,
                                               ChordAnnotationOptions options)
{
    switch (options.mode)
    {
    case ChordAnnotationNaive:
        return buildNaiveChordAnnotations(song, bars, options);
    }
    return buildNaiveChordAnnotations(song, bars, options);
}
