#include <iostream>

#include "ChordTimeline.h"

namespace {
int failures = 0;

void expectInt(const char *name, int actual, int expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

MidiEventRecord noteRecord(qint64 tick, int pitch)
{
    MidiEventRecord record;
    record.absoluteTick = tick;
    record.channel = 0;
    record.event.noteOnEvent(0, 0, pitch, 80);
    record.event.setAbsoluteTime(static_cast<int>(tick));
    return record;
}

MidiEventRecord eofRecord(qint64 tick)
{
    MidiEventRecord record;
    record.absoluteTick = tick;
    record.event.setType(MIDI_PB_EOF);
    return record;
}

void testBuildTimeline()
{
    SongData song;
    song.events.append(noteRecord(10, 60));
    song.events.append(noteRecord(50, 64));
    song.events.append(eofRecord(80));

    ChordTimeline timeline = buildChordTimeline(song, 0, PB_PART_both);
    expectInt("timeline count", timeline.chords.size(), 2);
    expectInt("first tick", static_cast<int>(timeline.chords[0].tick), 10);
    expectInt("second tick", static_cast<int>(timeline.chords[1].tick), 50);
}

void testIndexAtTick()
{
    ChordTimeline timeline;
    TimelineChord first;
    first.tick = 10;
    TimelineChord second;
    second.tick = 50;
    timeline.chords.append(first);
    timeline.chords.append(second);

    expectInt("before first", chordTimelineIndexAt(timeline, 0), 0);
    expectInt("at second", chordTimelineIndexAt(timeline, 50), 1);
    expectInt("after last", chordTimelineIndexAt(timeline, 70), 2);
}
}

int main()
{
    testBuildTimeline();
    testIndexAtTick();

    if (failures == 0) {
        std::cout << "ChordTimeline tests passed\n";
        return 0;
    }
    std::cerr << failures << " chord timeline test(s) failed\n";
    return 1;
}
