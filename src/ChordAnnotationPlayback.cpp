#include "ChordAnnotationPlayback.h"

#include "BarMap.h"
#include "Chord.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr int PitchClasses = 12;
constexpr int AnnotatedChordStreamIndex = -1001;
constexpr int RightHandRootBase = 55;
constexpr int CompingRightHandBase = 60;
constexpr int CompingBassHigh = 48;
constexpr int MaxCompingHits = 6;
constexpr int SimpleProStylePopRock = 1 << 16;

struct GrooveProfile
{
    double bpm = 120.0;
    double melodyDensity = 0.0;
    double syncopation = 0.0;
    double backbeatAccent = 1.0;
    double swing = 0.0;
    double alternatingBass = 0.0;
    double chordBeats = 4.0;
    int beatsPerBar = 4;
};

int normalizedPc(int pitch)
{
    int pc = pitch % PitchClasses;
    return pc < 0 ? pc + PitchClasses : pc;
}

int boundedMidiValue(int value)
{
    return qBound(0, value, 127);
}

int boundedTickTime(qint64 tick)
{
    if (tick <= 0)
        return 0;
    if (tick > std::numeric_limits<int>::max())
        return std::numeric_limits<int>::max();
    return static_cast<int>(tick);
}

int pitchForPitchClass(int pitchClass, int base)
{
    const int offset = (normalizedPc(pitchClass) - normalizedPc(base) + PitchClasses) % PitchClasses;
    return base + offset;
}

int pitchForPitchClassAtOrBelow(int pitchClass, int high)
{
    const int offset = (normalizedPc(high) - normalizedPc(pitchClass) + PitchClasses) % PitchClasses;
    return high - offset;
}

void appendUniquePitch(QVector<int>& pitches, int pitch)
{
    if (pitch >= 0 && pitch < MAX_MIDI_NOTES && !pitches.contains(pitch))
        pitches.append(pitch);
}

QVector<int> chordIntervals(const QString& suffix)
{
    if (suffix.startsWith(QStringLiteral("m7b5")))
        return {0, 3, 6, 10};
    if (suffix.startsWith(QStringLiteral("dim7")))
        return {0, 3, 6, 9};
    if (suffix.startsWith(QStringLiteral("maj7")))
        return {0, 4, 7, 11};
    if (suffix.startsWith(QStringLiteral("m7")))
        return {0, 3, 7, 10};
    if (suffix.startsWith(QStringLiteral("m6")))
        return {0, 3, 7, 9};
    if (suffix.startsWith(QStringLiteral("7")))
        return {0, 4, 7, 10};
    if (suffix.startsWith(QStringLiteral("6")))
        return {0, 4, 7, 9};
    if (suffix.startsWith(QStringLiteral("m")))
        return {0, 3, 7};
    if (suffix.startsWith(QStringLiteral("dim")))
        return {0, 3, 6};
    if (suffix.startsWith(QStringLiteral("aug")))
        return {0, 4, 8};
    if (suffix.startsWith(QStringLiteral("sus4")))
        return {0, 5, 7};
    if (suffix.startsWith(QStringLiteral("sus2")))
        return {0, 2, 7};
    return {0, 4, 7};
}

void appendExtensionIntervals(QVector<int>& intervals, const QString& suffix)
{
    if (suffix.contains(QStringLiteral("b9")))
        intervals.append(13);
    else if (suffix.contains(QStringLiteral("#9")))
        intervals.append(15);
    else if (suffix.contains(QStringLiteral("9")))
        intervals.append(14);
    if (suffix.contains(QStringLiteral("#11")))
        intervals.append(18);
    else if (suffix.contains(QStringLiteral("11")))
        intervals.append(17);
    if (suffix.contains(QStringLiteral("b13")))
        intervals.append(20);
    else if (suffix.contains(QStringLiteral("13")))
        intervals.append(21);
}

void removeIntervalWhileTooLarge(QVector<int>& intervals, int interval)
{
    while (intervals.size() > 4)
    {
        if (!intervals.removeOne(interval))
            return;
    }
}

void trimCompingIntervals(QVector<int>& intervals)
{
    removeIntervalWhileTooLarge(intervals, 7);
    removeIntervalWhileTooLarge(intervals, 0);
    while (intervals.size() > 4)
        intervals.removeLast();
}

QVector<int> rightHandCompingPitches(const ChordAnnotation& annotation)
{
    QVector<int> pitches;
    QVector<int> intervals = chordIntervals(annotation.suffix);
    appendExtensionIntervals(intervals, annotation.suffix);
    trimCompingIntervals(intervals);
    for (int interval : intervals)
    {
        int pitch = pitchForPitchClass(annotation.rootPitchClass + interval,
                                       CompingRightHandBase);
        while (pitch < 57)
            pitch += MIDI_OCTAVE;
        while (pitch > 79)
            pitch -= MIDI_OCTAVE;
        appendUniquePitch(pitches, pitch);
    }
    std::sort(pitches.begin(), pitches.end());
    return pitches;
}

QVector<int> compingChordPitches(const ChordAnnotation& annotation, bool strong)
{
    QVector<int> pitches;
    if (annotation.label.isEmpty() || annotation.rootPitchClass < 0)
        return pitches;
    if (strong)
    {
        const int bass = annotation.bassPitchClass >= 0 ?
                    annotation.bassPitchClass : annotation.rootPitchClass;
        appendUniquePitch(pitches, pitchForPitchClassAtOrBelow(bass, CompingBassHigh));
    }
    for (int pitch : rightHandCompingPitches(annotation))
        appendUniquePitch(pitches, pitch);
    return pitches;
}

bool seventhSuffix(const QString& suffix)
{
    return suffix.startsWith(QStringLiteral("7")) ||
            suffix.startsWith(QStringLiteral("maj7")) ||
            suffix.startsWith(QStringLiteral("m7")) ||
            suffix.startsWith(QStringLiteral("m7b5")) ||
            suffix.startsWith(QStringLiteral("dim7"));
}

QVector<int> proChordIntervals(const QString& suffix, bool shell)
{
    if (shell && seventhSuffix(suffix))
    {
        if (suffix.startsWith(QStringLiteral("m")) || suffix.startsWith(QStringLiteral("dim")))
            return suffix.startsWith(QStringLiteral("dim7")) ? QVector<int>{3, 9} : QVector<int>{3, 10};
        if (suffix.startsWith(QStringLiteral("maj7")))
            return {4, 11};
        return {4, 10};
    }
    QVector<int> intervals = chordIntervals(suffix);
    appendExtensionIntervals(intervals, suffix);
    trimCompingIntervals(intervals);
    return intervals;
}

QVector<int> voicingCandidate(int root, const QVector<int>& intervals, int base)
{
    QVector<int> candidate;
    for (int interval : intervals)
        appendUniquePitch(candidate, pitchForPitchClass(root + interval, base));
    std::sort(candidate.begin(), candidate.end());
    while (!candidate.isEmpty() && candidate.first() < 52)
        for (int& pitch : candidate)
            pitch += MIDI_OCTAVE;
    while (!candidate.isEmpty() && candidate.last() > 81)
        for (int& pitch : candidate)
            pitch -= MIDI_OCTAVE;
    return candidate;
}

double voicingScore(const QVector<int>& candidate, const QVector<int>& previous)
{
    if (candidate.isEmpty())
        return 1.0e9;
    double score = candidate.last() - candidate.first();
    const double center = (candidate.first() + candidate.last()) * 0.5;
    score += std::abs(center - 66.0) * 0.25;
    if (previous.isEmpty())
        return score;
    for (int i = 0; i < candidate.size(); i++)
        score += std::abs(candidate[i] - previous[std::min(i, previous.size() - 1)]) * 1.5;
    return score + std::abs(candidate.size() - previous.size()) * 8.0;
}

QVector<int> proChordVoicing(const ChordAnnotation& annotation,
                             const QVector<int>& previous, bool shell)
{
    const QVector<int> intervals = proChordIntervals(annotation.suffix, shell);
    QVector<int> best;
    double bestScore = 1.0e9;
    for (int base = 54; base <= 66; base += 4)
    {
        const QVector<int> candidate = voicingCandidate(annotation.rootPitchClass, intervals, base);
        const double score = voicingScore(candidate, previous);
        if (score < bestScore)
        {
            best = candidate;
            bestScore = score;
        }
    }
    return best;
}

QVector<int> proBassPitches(const ChordAnnotation& annotation, bool fifth)
{
    QVector<int> pitches;
    const int bass = annotation.bassPitchClass >= 0 ?
                annotation.bassPitchClass : annotation.rootPitchClass;
    appendUniquePitch(pitches, pitchForPitchClassAtOrBelow(bass, CompingBassHigh));
    if (fifth)
        appendUniquePitch(pitches, pitchForPitchClassAtOrBelow(annotation.rootPitchClass + 7,
                                                              CompingBassHigh + 7));
    return pitches;
}

MidiEventRecord chordEvent(qint64 tick, int channel, int pitch, bool on, int velocity)
{
    MidiEventRecord record;
    if (on)
        record.event.noteOnEvent(0, channel, pitch, boundedMidiValue(velocity));
    else
        record.event.noteOffEvent(0, channel, pitch, 0);
    record.event.setAbsoluteTime(boundedTickTime(tick));
    record.event.setTrack(channel);
    record.absoluteTick = qMax<qint64>(0, tick);
    record.streamIndex = AnnotatedChordStreamIndex;
    record.track = channel;
    record.channel = channel;
    return record;
}

int eventPriority(const MidiEventRecord& record)
{
    if (record.event.type() == MIDI_PB_EOF)
        return 3;
    if (record.streamIndex != AnnotatedChordStreamIndex)
        return 1;
    return record.event.type() == MIDI_NOTE_OFF ? 0 : 2;
}

qint64 beatLengthAt(const SongData& song, qint64 tick)
{
    int denominator = 4;
    for (const TimeSignatureChange& signature : song.timeSignatures)
    {
        if (signature.tick > tick)
            break;
        if (signature.denominator > 0)
            denominator = signature.denominator;
    }
    const int ppqn = song.ppqn > 0 ? song.ppqn : SongDataDefaultPpqn;
    return std::max<qint64>(1, static_cast<qint64>(ppqn) * 4 / denominator);
}

qint64 beatPart(qint64 beat, int divisor)
{
    return std::max<qint64>(1, beat / std::max(1, divisor));
}

qint64 beatFraction(qint64 beat, int numerator, int denominator)
{
    return std::max<qint64>(1, beat * numerator / std::max(1, denominator));
}

bool melodySource(const NoteEvent& note)
{
    if (note.channel == MIDI_DRUM_CHANNEL || note.pitch < 0 || note.pitch >= MAX_MIDI_NOTES)
        return false;
    if (note.hand == PB_PART_left)
        return false;
    return note.hand == PB_PART_right || note.pitch >= MIDDLE_C;
}

QVector<qint64> melodyOnsets(const SongData& song)
{
    QVector<qint64> onsets;
    for (const NoteEvent& note : song.notes)
        if (melodySource(note))
            onsets.append(note.startTick);
    std::sort(onsets.begin(), onsets.end());
    return onsets;
}

QVector<qint64> accentedMelodyOnsets(const SongData& song)
{
    double velocity = 0.0;
    int count = 0;
    for (const NoteEvent& note : song.notes)
        if (melodySource(note))
        {
            velocity += note.velocity;
            count++;
        }
    if (count == 0)
        return QVector<qint64>();
    QVector<qint64> onsets;
    const double threshold = velocity / count + 6.0;
    for (const NoteEvent& note : song.notes)
        if (melodySource(note) && note.velocity >= threshold)
            onsets.append(note.startTick);
    std::sort(onsets.begin(), onsets.end());
    return onsets.isEmpty() ? melodyOnsets(song) : onsets;
}

double tempoBpm(const SongData& song)
{
    for (const TempoChange& tempo : song.tempos)
        if (tempo.microsecondsPerQuarter > 0)
            return 60000000.0 / static_cast<double>(tempo.microsecondsPerQuarter);
    return 120.0;
}

int beatsPerBarAt(const SongData& song, qint64 tick)
{
    int beats = 4;
    for (const TimeSignatureChange& signature : song.timeSignatures)
    {
        if (signature.tick > tick)
            break;
        if (signature.numerator > 0)
            beats = signature.numerator;
    }
    return beats;
}

bool nearTick(qint64 value, qint64 target, qint64 tolerance)
{
    return std::abs(value - target) <= tolerance;
}

double averageChordBeats(const SongData& song, qint64 beat)
{
    if (song.chordAnnotations.isEmpty())
        return 4.0;
    double total = 0.0;
    for (const ChordAnnotation& annotation : song.chordAnnotations)
        total += static_cast<double>(annotation.endTick - annotation.startTick) /
                static_cast<double>(beat);
    return total / static_cast<double>(song.chordAnnotations.size());
}

void addRhythmEvidence(const SongData& song, const NoteEvent& note,
                       GrooveProfile& profile, double& velocitySum,
                       double& backbeatVelocity)
{
    const qint64 beat = beatLengthAt(song, note.startTick);
    const qint64 rem = note.startTick % beat;
    const qint64 tolerance = beatPart(beat, 8);
    const int beatIndex = static_cast<int>((note.startTick / beat) % profile.beatsPerBar);
    const qint64 beatTolerance = beatPart(beat, 6);
    profile.syncopation += !nearTick(rem, 0, beatTolerance) &&
            !nearTick(rem, beat, beatTolerance) ? 1.0 : 0.0;
    profile.swing += nearTick(rem, beatFraction(beat, 2, 3), tolerance) ? 1.0 : 0.0;
    if (beatIndex == 1 || beatIndex == 3)
        backbeatVelocity += note.velocity;
    velocitySum += note.velocity;
}

double alternatingBassEvidence(const SongData& song)
{
    int strong = 0;
    int total = 0;
    for (const NoteEvent& note : song.notes)
    {
        if (note.pitch >= MIDDLE_C || note.channel == MIDI_DRUM_CHANNEL)
            continue;
        const qint64 beat = beatLengthAt(song, note.startTick);
        const qint64 rem = note.startTick % beat;
        const qint64 beatTolerance = beatPart(beat, 6);
        if (!nearTick(rem, 0, beatTolerance) && !nearTick(rem, beat, beatTolerance))
            continue;
        total++;
        const int beatIndex = static_cast<int>((note.startTick / beat) % beatsPerBarAt(song, note.startTick));
        if (beatIndex == 0 || beatIndex == 2)
            strong++;
    }
    return total == 0 ? 0.0 : static_cast<double>(strong) / static_cast<double>(total);
}

GrooveProfile analyzeGroove(const SongData& song)
{
    GrooveProfile profile;
    const qint64 beat = beatLengthAt(song, 0);
    profile.bpm = tempoBpm(song);
    profile.beatsPerBar = beatsPerBarAt(song, 0);
    profile.chordBeats = averageChordBeats(song, beat);
    double velocitySum = 0.0;
    double backbeatVelocity = 0.0;
    int melodyCount = 0;
    for (const NoteEvent& note : song.notes)
        if (melodySource(note))
        {
            addRhythmEvidence(song, note, profile, velocitySum, backbeatVelocity);
            melodyCount++;
        }
    const double beats = std::max(1.0, static_cast<double>(song.durationTicks) / beat);
    profile.melodyDensity = static_cast<double>(melodyCount) / beats;
    profile.syncopation = melodyCount == 0 ? 0.0 : profile.syncopation / melodyCount;
    profile.swing = melodyCount == 0 ? 0.0 : profile.swing / melodyCount;
    profile.backbeatAccent = velocitySum <= 0.0 ? 1.0 : (backbeatVelocity * 2.0) / velocitySum;
    profile.alternatingBass = alternatingBassEvidence(song);
    return profile;
}

int validProStyleMask(int mask)
{
    mask &= AnnotatedChordProStyleAll;
    return mask == 0 ? AnnotatedChordProStyleSimple : mask;
}

int chooseSimpleProStyle(const GrooveProfile& profile)
{
    if (profile.beatsPerBar != 4)
        return AnnotatedChordProStyleBallad;
    if (profile.alternatingBass > 0.55 && profile.bpm >= 75.0 &&
            profile.bpm <= 155.0 && profile.swing < 0.35)
        return AnnotatedChordProStyleStride;
    if (profile.swing > 0.25 && profile.syncopation > 0.35)
        return AnnotatedChordProStyleContemporaryJazz;
    if (profile.syncopation > 0.45 && profile.backbeatAccent < 1.08 &&
            profile.bpm >= 80.0 && profile.bpm <= 180.0)
        return AnnotatedChordProStyleLatin;
    if (profile.backbeatAccent >= 1.08 && profile.swing < 0.35)
        return SimpleProStylePopRock;
    if (profile.bpm < 75.0 || profile.melodyDensity < 0.45 || profile.chordBeats >= 4.0)
        return AnnotatedChordProStyleBallad;
    return profile.syncopation > 0.40 ?
                AnnotatedChordProStyleContemporaryJazz : SimpleProStylePopRock;
}

QVector<int> configuredStyles(int mask, const GrooveProfile& profile)
{
    QVector<int> styles;
    mask = validProStyleMask(mask);
    if ((mask & AnnotatedChordProStyleSimple) != 0)
        styles.append(chooseSimpleProStyle(profile));
    for (int bit = AnnotatedChordProStyleBlues; bit <= AnnotatedChordProStyleContemporaryJazz; bit <<= 1)
        if ((mask & bit) != 0 && !styles.contains(bit))
            styles.append(bit);
    return styles.isEmpty() ? QVector<int>{chooseSimpleProStyle(profile)} : styles;
}

double styleFitness(const GrooveProfile& profile, int style)
{
    if (style == AnnotatedChordProStyleWaltz)
        return profile.beatsPerBar == 3 ? 3.0 : -2.0;
    if (style == AnnotatedChordProStyleBallad || style == AnnotatedChordProStyleNewAge)
        return 1.2 + (profile.bpm < 80.0 ? 1.0 : 0.0) + profile.chordBeats * 0.15;
    if (style == AnnotatedChordProStyleBlues || style == AnnotatedChordProStyleStride)
        return profile.alternatingBass + profile.swing + 0.5;
    if (style == AnnotatedChordProStyleFunk || style == AnnotatedChordProStyleReggae)
        return profile.syncopation * 1.5 + profile.melodyDensity * 0.25;
    if (style == AnnotatedChordProStyleGospel || style == AnnotatedChordProStyleSoul)
        return profile.backbeatAccent + profile.syncopation + 0.3;
    if (style == AnnotatedChordProStyleLatin || style == AnnotatedChordProStyleContemporaryJazz)
        return profile.syncopation + profile.swing + 0.6;
    if (style == AnnotatedChordProStyleMarch || style == AnnotatedChordProStyleCountry)
        return profile.alternatingBass + (1.0 - profile.syncopation);
    if (style == SimpleProStylePopRock || style == AnnotatedChordProStyleRockBallad)
        return profile.backbeatAccent + profile.chordBeats * 0.1;
    return 1.0 - profile.syncopation;
}

int stableStyleNoise(qint64 tick, int seed, int salt)
{
    quint32 value = static_cast<quint32>(tick) ^ static_cast<quint32>(seed * 1103515245u);
    value ^= static_cast<quint32>(salt * 2654435761u);
    value ^= value >> 16;
    return static_cast<int>(value & 0xffff);
}

double styleWeight(const GrooveProfile& profile, int style)
{
    return std::max(0.15, styleFitness(profile, style) + 2.0);
}

int chooseConfiguredStyle(const GrooveProfile& profile, qint64 seedTick,
                          int seed, int styleMask, int avoidStyle)
{
    const QVector<int> styles = configuredStyles(styleMask, profile);
    if (styles.size() == 1)
        return styles.first();
    QVector<double> weights;
    double total = 0.0;
    for (int style : styles)
    {
        const double weight = styleWeight(profile, style);
        weights.append(weight);
        total += weight;
    }
    double pick = static_cast<double>(stableStyleNoise(seedTick, seed, styleMask)) *
            total / 65536.0;
    int index = styles.size() - 1;
    for (int i = 0; i < weights.size(); i++)
    {
        pick -= weights[i];
        if (pick < 0.0)
        {
            index = i;
            break;
        }
    }
    if (styles[index] == avoidStyle)
        index = (index + 1) % styles.size();
    return styles[index];
}

qint64 quantizedTick(qint64 tick, qint64 start, qint64 grid)
{
    if (grid <= 0)
        return tick;
    const qint64 offset = std::max<qint64>(0, tick - start);
    return start + ((offset + grid / 2) / grid) * grid;
}

void appendCompingHit(QVector<qint64>& hits, qint64 tick, qint64 start, qint64 end)
{
    if (tick >= start && tick < end)
        hits.append(tick);
}

QVector<qint64> normalizedCompingHits(QVector<qint64> hits, qint64 minSpacing)
{
    std::sort(hits.begin(), hits.end());
    QVector<qint64> result;
    for (qint64 hit : hits)
    {
        if (!result.isEmpty() && hit - result.last() < minSpacing)
            continue;
        result.append(hit);
        if (result.size() >= MaxCompingHits)
            break;
    }
    return result;
}

QVector<qint64> compingHitsFor(const QVector<qint64>& melodyOnsets,
                               const SongData& song, const ChordAnnotation& annotation)
{
    const qint64 beat = beatLengthAt(song, annotation.startTick);
    const qint64 grid = std::max<qint64>(1, beat / 2);
    QVector<qint64> hits;
    appendCompingHit(hits, annotation.startTick, annotation.startTick, annotation.endTick);
    auto it = std::lower_bound(melodyOnsets.begin(), melodyOnsets.end(), annotation.startTick);
    for (; it != melodyOnsets.end() && *it < annotation.endTick; ++it)
        appendCompingHit(hits, quantizedTick(*it, annotation.startTick, grid),
                         annotation.startTick, annotation.endTick);
    if (annotation.endTick - annotation.startTick >= 2 * beat)
        appendCompingHit(hits, annotation.startTick + beat + grid,
                         annotation.startTick, annotation.endTick);
    if (annotation.endTick - annotation.startTick >= 3 * beat)
        appendCompingHit(hits, annotation.startTick + 2 * beat,
                         annotation.startTick, annotation.endTick);
    return normalizedCompingHits(hits, grid);
}

qint64 compingHitEnd(const QVector<qint64>& hits, int index, qint64 end, qint64 beat)
{
    qint64 off = std::min(end, hits[index] + std::max<qint64>(1, beat * 3 / 4));
    if (index + 1 < hits.size() && off > hits[index + 1])
        off = std::max(hits[index] + beatPart(beat, 4), hits[index + 1] - beatPart(beat, 8));
    return std::min(off, end);
}

void appendChordHit(QVector<MidiEventRecord>& events, qint64 start, qint64 end,
                    const QVector<int>& pitches, int channel, int velocity)
{
    if (end <= start)
        return;
    for (int pitch : pitches)
        events.append(chordEvent(start, channel, pitch, true, velocity));
    for (int pitch : pitches)
        events.append(chordEvent(end, channel, pitch, false, velocity));
}

void appendProHit(QVector<MidiEventRecord>& events, const ChordAnnotation& annotation,
                  QVector<int>& previousVoicing, qint64 tick, qint64 duration,
                  int channel, int velocity, int flags)
{
    if (annotation.label.isEmpty() || annotation.rootPitchClass < 0 ||
            tick < annotation.startTick || tick >= annotation.endTick)
        return;
    duration = std::min(duration, annotation.endTick - tick);
    QVector<int> pitches;
    if ((flags & 1) != 0)
        for (int pitch : proBassPitches(annotation, (flags & 4) != 0))
            appendUniquePitch(pitches, pitch);
    if ((flags & 2) != 0)
    {
        previousVoicing = proChordVoicing(annotation, previousVoicing, (flags & 8) != 0);
        for (int pitch : previousVoicing)
            appendUniquePitch(pitches, pitch);
    }
    appendChordHit(events, tick, tick + duration, pitches, channel, velocity);
}

void appendRagtimeComping(QVector<MidiEventRecord>& events, const ChordAnnotation& annotation,
                          QVector<int>& previousVoicing, int channel, int velocity, qint64 beat)
{
    int index = 0;
    for (qint64 tick = annotation.startTick; tick < annotation.endTick; tick += beat, index++)
    {
        const bool bass = (index % 2) == 0;
        appendProHit(events, annotation, previousVoicing, tick,
                     bass ? beatPart(beat, 2) : beatPart(beat, 3), channel,
                     bass ? velocity : qMax(1, velocity - 6), bass ? 1 : 2);
    }
}

void appendPopComping(QVector<MidiEventRecord>& events, const ChordAnnotation& annotation,
                      QVector<int>& previousVoicing, const GrooveProfile& profile,
                      int channel, int velocity, qint64 beat)
{
    int index = 0;
    for (qint64 tick = annotation.startTick; tick < annotation.endTick; tick += beat, index++)
    {
        if (index % 4 == 0)
            appendProHit(events, annotation, previousVoicing, tick, beatPart(beat, 2),
                         channel, velocity, 1 | 4);
        if (index % 4 == 1 || index % 4 == 3)
            appendProHit(events, annotation, previousVoicing, tick, beatPart(beat, 2),
                         channel, velocity, 2);
        if (profile.melodyDensity > 1.4)
            appendProHit(events, annotation, previousVoicing,
                         tick + beatPart(beat, 2), beatPart(beat, 3),
                         channel, qMax(1, velocity - 10), 2);
    }
}

QVector<qint64> proAccentHits(const QVector<qint64>& onsets, const ChordAnnotation& annotation,
                              qint64 beat)
{
    QVector<qint64> hits;
    const qint64 grid = beatPart(beat, 2);
    appendCompingHit(hits, annotation.startTick + grid, annotation.startTick, annotation.endTick);
    appendCompingHit(hits, annotation.startTick + beat + grid, annotation.startTick, annotation.endTick);
    auto it = std::lower_bound(onsets.begin(), onsets.end(), annotation.startTick);
    for (; it != onsets.end() && *it < annotation.endTick; ++it)
        appendCompingHit(hits, quantizedTick(*it, annotation.startTick, grid),
                         annotation.startTick, annotation.endTick);
    return normalizedCompingHits(hits, grid);
}

void appendJazzComping(QVector<MidiEventRecord>& events, const ChordAnnotation& annotation,
                       const QVector<qint64>& onsets, QVector<int>& previousVoicing,
                       int channel, int velocity, qint64 beat)
{
    const QVector<qint64> hits = proAccentHits(onsets, annotation, beat);
    for (int i = 0; i < hits.size(); i++)
        appendProHit(events, annotation, previousVoicing, hits[i], beatPart(beat, 2),
                     channel, i == 0 ? velocity : qMax(1, velocity - 8),
                     (i == 0 ? 1 : 0) | 2 | 8);
}

void appendBossaOffset(QVector<MidiEventRecord>& events, const ChordAnnotation& annotation,
                       QVector<int>& previousVoicing, qint64 base, qint64 offset,
                       qint64 beat, int channel, int velocity, int flags)
{
    const qint64 tick = base + offset;
    if (tick >= annotation.startTick && tick < annotation.endTick)
        appendProHit(events, annotation, previousVoicing, tick, beatPart(beat, 2),
                     channel, velocity, flags);
}

void appendBossaComping(QVector<MidiEventRecord>& events, const ChordAnnotation& annotation,
                        QVector<int>& previousVoicing, int channel, int velocity, qint64 beat)
{
    const qint64 bar = 4 * beat;
    for (qint64 base = annotation.startTick; base < annotation.endTick; base += bar)
    {
        appendBossaOffset(events, annotation, previousVoicing, base, 0, beat, channel, velocity, 1);
        appendBossaOffset(events, annotation, previousVoicing, base,
                          beatFraction(beat, 3, 2), beat, channel, velocity - 6, 1);
        appendBossaOffset(events, annotation, previousVoicing, base, beat * 2, beat, channel, velocity, 1);
        for (qint64 off : {beatPart(beat, 2), beat, beatFraction(beat, 5, 2),
                           beatFraction(beat, 7, 2)})
            appendBossaOffset(events, annotation, previousVoicing, base, off, beat,
                              channel, qMax(1, velocity - 8), 2);
    }
}

void appendBalladComping(QVector<MidiEventRecord>& events, const ChordAnnotation& annotation,
                         QVector<int>& previousVoicing, int channel, int velocity, qint64 beat)
{
    appendProHit(events, annotation, previousVoicing, annotation.startTick,
                 std::min(annotation.endTick - annotation.startTick, beat * 2),
                 channel, velocity, 1);
    if (annotation.startTick + beat < annotation.endTick)
        appendProHit(events, annotation, previousVoicing, annotation.startTick + beat,
                     std::min(annotation.endTick - annotation.startTick - beat, beat * 2),
                     channel, qMax(1, velocity - 8), 2);
}

void appendBluesComping(QVector<MidiEventRecord>& events, const ChordAnnotation& annotation,
                        QVector<int>& previousVoicing, int channel, int velocity, qint64 beat)
{
    const int bass = annotation.bassPitchClass >= 0 ? annotation.bassPitchClass : annotation.rootPitchClass;
    const QVector<int> bassLine = {bass, annotation.rootPitchClass + 7,
                                   annotation.rootPitchClass + 9, annotation.rootPitchClass + 10};
    int index = 0;
    const qint64 halfBeat = beatPart(beat, 2);
    const qint64 thirdBeat = beatPart(beat, 3);
    for (qint64 tick = annotation.startTick; tick < annotation.endTick; tick += halfBeat, index++)
    {
        QVector<int> bassPitch;
        appendUniquePitch(bassPitch, pitchForPitchClassAtOrBelow(bassLine[index % bassLine.size()],
                                                                 CompingBassHigh));
        appendChordHit(events, tick, std::min(tick + thirdBeat, annotation.endTick),
                       bassPitch, channel, velocity);
        if (index % 4 == 2)
            appendProHit(events, annotation, previousVoicing, tick + thirdBeat, thirdBeat,
                         channel, qMax(1, velocity - 8), 2 | 8);
    }
}

void appendGospelComping(QVector<MidiEventRecord>& events, const ChordAnnotation& annotation,
                         QVector<int>& previousVoicing, int channel, int velocity, qint64 beat)
{
    for (qint64 tick = annotation.startTick; tick < annotation.endTick; tick += beat)
    {
        appendProHit(events, annotation, previousVoicing, tick, beatPart(beat, 2),
                     channel, velocity, 1 | 4);
        appendProHit(events, annotation, previousVoicing,
                     tick + beatPart(beat, 2), beatPart(beat, 3),
                     channel, qMax(1, velocity - 5), 2);
        if (tick + beatFraction(beat, 3, 4) < annotation.endTick)
            appendProHit(events, annotation, previousVoicing,
                         tick + beatFraction(beat, 3, 4), beatPart(beat, 4),
                         channel, qMax(1, velocity - 12), 2);
    }
}

void appendFunkComping(QVector<MidiEventRecord>& events, const ChordAnnotation& annotation,
                       QVector<int>& previousVoicing, int channel, int velocity, qint64 beat)
{
    const qint64 quarterBeat = beatPart(beat, 4);
    const qint64 halfBeat = beatPart(beat, 2);
    appendProHit(events, annotation, previousVoicing, annotation.startTick, quarterBeat,
                 channel, qMax(1, velocity - 8), 1);
    for (qint64 tick = annotation.startTick + quarterBeat; tick < annotation.endTick; tick += halfBeat)
        appendProHit(events, annotation, previousVoicing, tick, beatPart(beat, 5),
                     channel, velocity, 2 | 8);
}

void appendSoulComping(QVector<MidiEventRecord>& events, const ChordAnnotation& annotation,
                       QVector<int>& previousVoicing, int channel, int velocity, qint64 beat)
{
    const qint64 halfBeat = beatPart(beat, 2);
    appendProHit(events, annotation, previousVoicing, annotation.startTick, halfBeat,
                 channel, velocity, 1);
    for (qint64 tick = annotation.startTick + halfBeat; tick < annotation.endTick; tick += beat)
        appendProHit(events, annotation, previousVoicing, tick, halfBeat,
                     channel, qMax(1, velocity - 5), 2);
}

void appendBrokenChordTexture(QVector<MidiEventRecord>& events, const ChordAnnotation& annotation,
                              QVector<int>& previousVoicing, int channel, int velocity,
                              qint64 beat, bool ambient)
{
    QVector<int> pitches = proBassPitches(annotation, false);
    previousVoicing = proChordVoicing(annotation, previousVoicing, false);
    for (int pitch : previousVoicing)
        appendUniquePitch(pitches, pitch);
    if (pitches.isEmpty())
        return;
    const qint64 step = ambient ? beatPart(beat, 2) : beatPart(beat, 4);
    for (qint64 tick = annotation.startTick, i = 0; tick < annotation.endTick; tick += step, i++)
        appendChordHit(events, tick, std::min(tick + step, annotation.endTick),
                       QVector<int>{pitches[i % pitches.size()]},
                       channel, qMax(1, velocity - (ambient ? 12 : 4)));
}

void appendWaltzComping(QVector<MidiEventRecord>& events, const ChordAnnotation& annotation,
                        QVector<int>& previousVoicing, int channel, int velocity, qint64 beat)
{
    for (qint64 tick = annotation.startTick; tick < annotation.endTick; tick += 3 * beat)
    {
        appendProHit(events, annotation, previousVoicing, tick, beatPart(beat, 2),
                     channel, velocity, 1);
        appendProHit(events, annotation, previousVoicing, tick + beat, beatPart(beat, 2),
                     channel, qMax(1, velocity - 8), 2);
        appendProHit(events, annotation, previousVoicing, tick + 2 * beat, beatPart(beat, 2),
                     channel, qMax(1, velocity - 10), 2);
    }
}

void appendReggaeComping(QVector<MidiEventRecord>& events, const ChordAnnotation& annotation,
                         QVector<int>& previousVoicing, int channel, int velocity, qint64 beat)
{
    for (qint64 tick = annotation.startTick + beatPart(beat, 2);
         tick < annotation.endTick; tick += beat)
        appendProHit(events, annotation, previousVoicing, tick, beatPart(beat, 4),
                     channel, velocity, 2);
}

void appendRockBalladComping(QVector<MidiEventRecord>& events, const ChordAnnotation& annotation,
                             QVector<int>& previousVoicing, int channel, int velocity, qint64 beat)
{
    for (qint64 tick = annotation.startTick; tick < annotation.endTick; tick += beatPart(beat, 2))
        appendProHit(events, annotation, previousVoicing, tick, beatPart(beat, 3),
                     channel, tick == annotation.startTick ? velocity : qMax(1, velocity - 8),
                     tick == annotation.startTick ? 1 | 4 : 2);
}

QVector<MidiEventRecord> buildRootChordEvents(const SongData& song, int channel, int velocity)
{
    QVector<MidiEventRecord> events;
    for (const ChordAnnotation& annotation : song.chordAnnotations)
    {
        if (annotation.endTick <= annotation.startTick)
            continue;
        const QVector<int> pitches = annotatedChordPitches(annotation);
        appendChordHit(events, annotation.startTick, annotation.endTick, pitches, channel, velocity);
    }
    return events;
}

QVector<MidiEventRecord> buildCompingEvents(const SongData& song, int channel, int velocity)
{
    QVector<MidiEventRecord> events;
    const QVector<qint64> onsets = accentedMelodyOnsets(song);
    for (const ChordAnnotation& annotation : song.chordAnnotations)
    {
        const qint64 beat = beatLengthAt(song, annotation.startTick);
        const QVector<qint64> hits = compingHitsFor(onsets, song, annotation);
        for (int i = 0; i < hits.size(); i++)
        {
            const bool strong = ((hits[i] - annotation.startTick) % beat) == 0;
            appendChordHit(events, hits[i], compingHitEnd(hits, i, annotation.endTick, beat),
                           compingChordPitches(annotation, strong), channel,
                           strong ? velocity : qMax(1, velocity - 10));
        }
    }
    return events;
}

void appendProCompingForStyle(QVector<MidiEventRecord>& events, const SongData& song,
                              const ChordAnnotation& annotation, const QVector<qint64>& onsets,
                              QVector<int>& previousVoicing, const GrooveProfile& profile,
                              int style, int channel, int velocity)
{
    const qint64 beat = beatLengthAt(song, annotation.startTick);
    if (style == SimpleProStylePopRock)
        appendPopComping(events, annotation, previousVoicing, profile, channel, velocity, beat);
    else if (style == AnnotatedChordProStyleBlues)
        appendBluesComping(events, annotation, previousVoicing, channel, velocity, beat);
    else if (style == AnnotatedChordProStyleStride)
        appendRagtimeComping(events, annotation, previousVoicing, channel, velocity, beat);
    else if (style == AnnotatedChordProStyleGospel)
        appendGospelComping(events, annotation, previousVoicing, channel, velocity, beat);
    else if (style == AnnotatedChordProStyleFunk)
        appendFunkComping(events, annotation, previousVoicing, channel, velocity, beat);
    else if (style == AnnotatedChordProStyleSoul)
        appendSoulComping(events, annotation, previousVoicing, channel, velocity, beat);
    else if (style == AnnotatedChordProStyleClassical)
        appendBrokenChordTexture(events, annotation, previousVoicing, channel, velocity, beat, false);
    else if (style == AnnotatedChordProStyleWaltz)
        appendWaltzComping(events, annotation, previousVoicing, channel, velocity, beat);
    else if (style == AnnotatedChordProStyleMarch)
        appendRagtimeComping(events, annotation, previousVoicing, channel, velocity, beat);
    else if (style == AnnotatedChordProStyleLatin)
        appendBossaComping(events, annotation, previousVoicing, channel, velocity, beat);
    else if (style == AnnotatedChordProStyleReggae)
        appendReggaeComping(events, annotation, previousVoicing, channel, velocity, beat);
    else if (style == AnnotatedChordProStyleCountry)
        appendPopComping(events, annotation, previousVoicing, profile, channel, velocity, beat);
    else if (style == AnnotatedChordProStyleNewAge)
        appendBrokenChordTexture(events, annotation, previousVoicing, channel, velocity, beat, true);
    else if (style == AnnotatedChordProStyleRockBallad)
        appendRockBalladComping(events, annotation, previousVoicing, channel, velocity, beat);
    else if (style == AnnotatedChordProStyleContemporaryJazz)
        appendJazzComping(events, annotation, onsets, previousVoicing, channel, velocity, beat);
    else
        appendBalladComping(events, annotation, previousVoicing, channel, velocity, beat);
}

QVector<MidiEventRecord> buildProCompingEvents(const SongData& song, int channel,
                                               int velocity, int styleMask)
{
    QVector<MidiEventRecord> events;
    const GrooveProfile profile = analyzeGroove(song);
    const QVector<qint64> onsets = melodyOnsets(song);
    qint64 duration = song.durationTicks;
    for (const ChordAnnotation& annotation : song.chordAnnotations)
        duration = std::max(duration, annotation.endTick);
    const BarMap bars = buildBarMap(song.ppqn, duration, song.timeSignatures);
    QVector<int> barStyles(bars.barStarts.size(), 0);
    QVector<int> previousVoicing;
    int previousBar = -1;
    int previousStyle = 0;
    for (const ChordAnnotation& annotation : song.chordAnnotations)
        if (annotation.endTick > annotation.startTick)
        {
            const int bar = barAtTick(bars, annotation.startTick);
            if (barStyles[bar] == 0)
                barStyles[bar] = chooseConfiguredStyle(
                            profile, bars.barStarts[bar], bar, styleMask, previousStyle);
            if (bar != previousBar)
            {
                previousBar = bar;
                previousStyle = barStyles[bar];
            }
            const int style = barStyles[bar];
            appendProCompingForStyle(events, song, annotation, onsets, previousVoicing,
                                     profile, style, channel, velocity);
        }
    return events;
}
}

int annotatedChordPlaybackChannel(const SongData& song)
{
    return annotatedChordPlaybackChannel(song, QVector<int>());
}

int annotatedChordPlaybackChannel(const SongData& song,
                                  const QVector<int>& excludedChannels)
{
    bool used[MAX_MIDI_CHANNELS] = {};
    for (const MidiEventRecord& record : song.events)
        if (record.channel >= 0 && record.channel < MAX_MIDI_CHANNELS)
            used[record.channel] = true;
    for (int channel : excludedChannels)
        if (channel >= 0 && channel < MAX_MIDI_CHANNELS)
            used[channel] = true;
    for (int channel = MAX_MIDI_CHANNELS - 1; channel >= 0; channel--)
        if (channel != MIDI_DRUM_CHANNEL && !used[channel])
            return channel;
    return -1;
}

QVector<int> annotatedChordPitches(const ChordAnnotation& annotation)
{
    QVector<int> pitches;
    if (annotation.label.isEmpty() || annotation.rootPitchClass < 0)
        return pitches;

    const int root = pitchForPitchClass(annotation.rootPitchClass, RightHandRootBase);
    const int bassPitchClass = annotation.bassPitchClass >= 0 ?
                annotation.bassPitchClass : annotation.rootPitchClass;
    appendUniquePitch(pitches, pitchForPitchClassAtOrBelow(bassPitchClass, root - MIDI_OCTAVE));
    QVector<int> intervals = chordIntervals(annotation.suffix);
    appendExtensionIntervals(intervals, annotation.suffix);
    for (int interval : intervals)
        appendUniquePitch(pitches, root + interval);
    return pitches;
}

QVector<MidiEventRecord> buildAnnotatedChordPlaybackEvents(const SongData& song,
                                                           int channel,
                                                           int velocity)
{
    return buildAnnotatedChordPlaybackEvents(song, channel, velocity,
                                            AnnotatedChordPlayRootChord);
}

QVector<MidiEventRecord> buildAnnotatedChordPlaybackEvents(const SongData& song,
                                                           int channel,
                                                           int velocity,
                                                           AnnotatedChordPlayMode mode)
{
    return buildAnnotatedChordPlaybackEvents(song, channel, velocity, mode,
                                            AnnotatedChordProStyleSimple);
}

QVector<MidiEventRecord> buildAnnotatedChordPlaybackEvents(const SongData& song,
                                                           int channel,
                                                           int velocity,
                                                           AnnotatedChordPlayMode mode,
                                                           int proStyleMask)
{
    QVector<MidiEventRecord> events;
    if (channel < 0 || channel >= MAX_MIDI_CHANNELS || channel == MIDI_DRUM_CHANNEL)
        return events;
    if (mode == AnnotatedChordPlayProComping)
        return buildProCompingEvents(song, channel, velocity, proStyleMask);
    if (mode == AnnotatedChordPlayComping)
        return buildCompingEvents(song, channel, velocity);
    return buildRootChordEvents(song, channel, velocity);
}

QVector<MidiEventRecord> buildPlaybackEventsWithAnnotatedChords(const SongData& song,
                                                                bool enabled)
{
    return buildPlaybackEventsWithAnnotatedChords(
                song, enabled, annotatedChordPlaybackChannel(song));
}

QVector<MidiEventRecord> buildPlaybackEventsWithAnnotatedChords(const SongData& song,
                                                                bool enabled,
                                                                int channel)
{
    return buildPlaybackEventsWithAnnotatedChords(song, enabled, channel,
                                                 AnnotatedChordPlayRootChord);
}

QVector<MidiEventRecord> buildPlaybackEventsWithAnnotatedChords(const SongData& song,
                                                                bool enabled,
                                                                int channel,
                                                                AnnotatedChordPlayMode mode)
{
    return buildPlaybackEventsWithAnnotatedChords(song, enabled, channel, mode,
                                                 AnnotatedChordProStyleSimple);
}

QVector<MidiEventRecord> buildPlaybackEventsWithAnnotatedChords(const SongData& song,
                                                                bool enabled,
                                                                int channel,
                                                                AnnotatedChordPlayMode mode,
                                                                int proStyleMask)
{
    QVector<MidiEventRecord> events = song.events;
    if (!enabled)
        return events;

    const QVector<MidiEventRecord> chordEvents =
            buildAnnotatedChordPlaybackEvents(song, channel,
                                             AnnotatedChordPlaybackVelocity, mode,
                                             proStyleMask);
    if (chordEvents.isEmpty())
        return events;
    events += chordEvents;
    std::stable_sort(events.begin(), events.end(),
                     [](const MidiEventRecord& left, const MidiEventRecord& right) {
        if (left.absoluteTick != right.absoluteTick)
            return left.absoluteTick < right.absoluteTick;
        return eventPriority(left) < eventPriority(right);
    });
    return events;
}
