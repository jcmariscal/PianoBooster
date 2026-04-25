#include <iostream>

#include "PlaybackCursor.h"

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

MidiEventRecord noteRecord(qint64 tick, int note)
{
    MidiEventRecord record;
    record.absoluteTick = tick;
    record.event.noteOnEvent(0, 0, note, 64);
    record.event.setAbsoluteTime(static_cast<int>(tick));
    return record;
}

void testReadFromStart()
{
    QVector<MidiEventRecord> events;
    events.append(noteRecord(10, 60));
    events.append(noteRecord(25, 62));
    PlaybackCursor cursor;
    CMidiEvent event;

    setPlaybackCursorEvents(cursor, &events);
    expectBool("first read", readPlaybackCursorEvent(cursor, event), true);
    expectInt("first delta", event.deltaTime(), 10);
    expectInt("first note", event.note(), 60);
    expectBool("second read", readPlaybackCursorEvent(cursor, event), true);
    expectInt("second delta", event.deltaTime(), 15);
    expectBool("past end", readPlaybackCursorEvent(cursor, event), false);
}

void testSeek()
{
    QVector<MidiEventRecord> events;
    events.append(noteRecord(10, 60));
    events.append(noteRecord(25, 62));
    PlaybackCursor cursor;
    CMidiEvent event;

    setPlaybackCursorEvents(cursor, &events);
    seekPlaybackCursor(cursor, 20);
    expectInt("seek index", playbackCursorIndex(cursor), 1);
    expectBool("seek read", readPlaybackCursorEvent(cursor, event), true);
    expectInt("seek delta", event.deltaTime(), 5);
    expectInt("seek note", event.note(), 62);
}

void testEmpty()
{
    PlaybackCursor cursor;
    CMidiEvent event;

    seekPlaybackCursor(cursor, 30);
    expectBool("empty read", readPlaybackCursorEvent(cursor, event), false);
}
}

int main()
{
    testReadFromStart();
    testSeek();
    testEmpty();

    if (failures == 0) {
        std::cout << "PlaybackCursor tests passed\n";
        return 0;
    }
    std::cerr << failures << " playback cursor test(s) failed\n";
    return 1;
}
