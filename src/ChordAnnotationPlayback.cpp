#include "ChordAnnotationPlayback.h"

#include "Chord.h"

#include <algorithm>
#include <limits>

namespace {
constexpr int PitchClasses = 12;
constexpr int AnnotatedChordStreamIndex = -1001;
constexpr int RightHandRootBase = 55;
constexpr int CompingRightHandBase = 60;
constexpr int CompingBassHigh = 48;
constexpr int MaxCompingHits = 6;

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

qint64 quantizedTick(qint64 tick, qint64 start, qint64 grid)
{
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
        off = std::max(hits[index] + std::max<qint64>(1, beat / 4), hits[index + 1] - beat / 8);
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
    const QVector<qint64> onsets = melodyOnsets(song);
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
    QVector<MidiEventRecord> events;
    if (channel < 0 || channel >= MAX_MIDI_CHANNELS || channel == MIDI_DRUM_CHANNEL)
        return events;
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
    QVector<MidiEventRecord> events = song.events;
    if (!enabled)
        return events;

    const QVector<MidiEventRecord> chordEvents =
            buildAnnotatedChordPlaybackEvents(song, channel,
                                             AnnotatedChordPlaybackVelocity, mode);
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
