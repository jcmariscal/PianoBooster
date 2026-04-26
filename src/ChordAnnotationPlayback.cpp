#include "ChordAnnotationPlayback.h"

#include <algorithm>
#include <limits>

namespace {
constexpr int PitchClasses = 12;
constexpr int AnnotatedChordStreamIndex = -1001;
constexpr int RightHandRootBase = 55;

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
    QVector<MidiEventRecord> events;
    if (channel < 0 || channel >= MAX_MIDI_CHANNELS || channel == MIDI_DRUM_CHANNEL)
        return events;

    for (const ChordAnnotation& annotation : song.chordAnnotations)
    {
        if (annotation.endTick <= annotation.startTick)
            continue;
        const QVector<int> pitches = annotatedChordPitches(annotation);
        for (int pitch : pitches)
            events.append(chordEvent(annotation.startTick, channel, pitch, true, velocity));
        for (int pitch : pitches)
            events.append(chordEvent(annotation.endTick, channel, pitch, false, velocity));
    }
    return events;
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
    QVector<MidiEventRecord> events = song.events;
    if (!enabled)
        return events;

    const QVector<MidiEventRecord> chordEvents =
            buildAnnotatedChordPlaybackEvents(song, channel, AnnotatedChordPlaybackVelocity);
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
