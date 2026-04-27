#include <iostream>

#include "ChordAnnotationPlayback.h"

namespace {
int failures = 0;

void expectBool(const char *name, bool actual, bool expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

void expectInt(const char *name, int actual, int expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

ChordAnnotation annotation(qint64 start, qint64 end, int root, const QString& suffix)
{
    ChordAnnotation chord;
    chord.startTick = start;
    chord.endTick = end;
    chord.label = QStringLiteral("x");
    chord.rootPitchClass = root;
    chord.suffix = suffix;
    return chord;
}

ChordAnnotation slashAnnotation(qint64 start, qint64 end, int root,
                                const QString& suffix, int bass)
{
    ChordAnnotation chord = annotation(start, end, root, suffix);
    chord.bassPitchClass = bass;
    return chord;
}

MidiEventRecord eofRecord(qint64 tick)
{
    MidiEventRecord record;
    record.absoluteTick = tick;
    record.event.setType(MIDI_PB_EOF);
    record.event.setAbsoluteTime(static_cast<int>(tick));
    return record;
}

NoteEvent note(int pitch, qint64 start, qint64 duration)
{
    NoteEvent event;
    event.startTick = start;
    event.endTick = start + duration;
    event.pitch = pitch;
    event.velocity = 96;
    event.channel = 0;
    event.track = 0;
    return event;
}

bool hasNoteOnAt(const QVector<MidiEventRecord>& events, qint64 tick)
{
    for (const MidiEventRecord& event : events)
        if (event.absoluteTick == tick && event.event.type() == MIDI_NOTE_ON)
            return true;
    return false;
}

bool hasNoteOffAt(const QVector<MidiEventRecord>& events, qint64 tick)
{
    for (const MidiEventRecord& event : events)
        if (event.absoluteTick == tick && event.event.type() == MIDI_NOTE_OFF)
            return true;
    return false;
}

void testChordPitches()
{
    ChordAnnotation chord = annotation(0, 96, 0, QString());
    QVector<int> pitches = annotatedChordPitches(chord);
    expectBool("C contains bass root", pitches.contains(48), true);
    expectBool("C contains chord root", pitches.contains(60), true);
    expectBool("C contains third", pitches.contains(64), true);
    expectBool("C contains fifth", pitches.contains(67), true);
    expectInt("C pitch count", pitches.size(), 4);

    chord = annotation(0, 96, 0, QStringLiteral("m"));
    pitches = annotatedChordPitches(chord);
    expectBool("Cm contains bass root", pitches.contains(48), true);
    expectBool("Cm contains chord root", pitches.contains(60), true);
    expectBool("Cm contains minor third", pitches.contains(63), true);
    expectBool("Cm contains fifth", pitches.contains(67), true);
    expectInt("Cm pitch count", pitches.size(), 4);

    chord = annotation(0, 96, 7, QStringLiteral("7"));
    pitches = annotatedChordPitches(chord);
    expectBool("G7 contains bass root", pitches.contains(43), true);
    expectBool("G7 contains chord root", pitches.contains(55), true);
    expectBool("G7 contains seventh", pitches.contains(65), true);
    expectInt("G7 pitch count", pitches.size(), 5);

    pitches = annotatedChordPitches(slashAnnotation(0, 96, 5, QString(), 9));
    expectBool("slash chord low bass", pitches.contains(45), true);
    expectBool("slash chord root chord", pitches.contains(65), true);
}

void testPlaybackEvents()
{
    SongData song;
    song.chordAnnotations.append(annotation(0, 96, 0, QString()));
    const QVector<MidiEventRecord> events = buildAnnotatedChordPlaybackEvents(
                song, 15, AnnotatedChordPlaybackVelocity);
    expectInt("triad event count", events.size(), 8);
    expectInt("first tick", static_cast<int>(events.first().absoluteTick), 0);
    expectInt("first type", events.first().event.type(), MIDI_NOTE_ON);
    expectInt("first velocity", events.first().event.velocity(), AnnotatedChordPlaybackVelocity);
    expectInt("last tick", static_cast<int>(events.last().absoluteTick), 96);
    expectInt("last type", events.last().event.type(), MIDI_NOTE_OFF);
}

void testCompingPlaybackEvents()
{
    SongData song;
    song.chordAnnotations.append(annotation(0, 4 * SongDataDefaultPpqn, 0, QString()));
    song.notes.append(note(72, SongDataDefaultPpqn + SongDataDefaultPpqn / 2, 12));
    const QVector<MidiEventRecord> events = buildAnnotatedChordPlaybackEvents(
                song, 15, AnnotatedChordPlaybackVelocity, AnnotatedChordPlayComping);
    expectBool("comping starts on downbeat", hasNoteOnAt(events, 0), true);
    expectBool("comping follows melody syncopation",
               hasNoteOnAt(events, SongDataDefaultPpqn + SongDataDefaultPpqn / 2), true);
    expectBool("comping releases short first hit",
               hasNoteOffAt(events, SongDataDefaultPpqn * 3 / 4), true);
}

void testMerge()
{
    SongData song;
    song.events.append(eofRecord(96));
    song.chordAnnotations.append(annotation(0, 96, 0, QString()));
    QVector<MidiEventRecord> events = buildPlaybackEventsWithAnnotatedChords(song, true);
    expectInt("merged count", events.size(), 9);
    expectInt("eof last", events.last().event.type(), MIDI_PB_EOF);
    events = buildPlaybackEventsWithAnnotatedChords(song, false);
    expectInt("disabled count", events.size(), 1);
}

void testMergeCompingMode()
{
    SongData song;
    song.events.append(eofRecord(4 * SongDataDefaultPpqn));
    song.chordAnnotations.append(annotation(0, 4 * SongDataDefaultPpqn, 0, QString()));
    song.notes.append(note(72, SongDataDefaultPpqn + SongDataDefaultPpqn / 2, 12));
    const QVector<MidiEventRecord> events = buildPlaybackEventsWithAnnotatedChords(
                song, true, 15, AnnotatedChordPlayComping);
    expectBool("merged comping note", hasNoteOnAt(events, SongDataDefaultPpqn * 3 / 2), true);
    expectInt("merged comping eof last", events.last().event.type(), MIDI_PB_EOF);
}

void testNoSpareChannelLeavesSongUntouched()
{
    SongData song;
    for (int channel = 0; channel < MAX_MIDI_CHANNELS; channel++)
    {
        MidiEventRecord record;
        record.channel = channel;
        record.event.noteOnEvent(0, channel, 60, 64);
        song.events.append(record);
    }
    song.chordAnnotations.append(annotation(0, 96, 0, QString()));
    expectInt("no spare channel", annotatedChordPlaybackChannel(song), -1);
    expectInt("no injected events", buildPlaybackEventsWithAnnotatedChords(song, true).size(),
              song.events.size());
}

void testExcludedChannels()
{
    SongData song;
    expectInt("last free channel", annotatedChordPlaybackChannel(song, QVector<int>()), 15);
    expectInt("excluded channel", annotatedChordPlaybackChannel(song, QVector<int>{15}), 14);
}
}

int main()
{
    testChordPitches();
    testPlaybackEvents();
    testCompingPlaybackEvents();
    testMerge();
    testMergeCompingMode();
    testNoSpareChannelLeavesSongUntouched();
    testExcludedChannels();

    if (failures == 0) {
        std::cout << "ChordAnnotationPlayback tests passed\n";
        return 0;
    }
    std::cerr << failures << " chord annotation playback test(s) failed\n";
    return 1;
}
