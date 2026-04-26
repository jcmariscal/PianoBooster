#include <iostream>

#include "SongData.h"

namespace {
int failures = 0;

void expectInt(const char *name, int actual, int expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

void expectBool(const char *name, bool actual, bool expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

void testSongDataDefaults()
{
    SongData song;

    expectInt("default ppqn", song.ppqn, SongDataDefaultPpqn);
    expectInt("default duration", static_cast<int>(song.durationTicks), 0);
    expectBool("events empty", song.events.isEmpty(), true);
    expectBool("notes empty", song.notes.isEmpty(), true);
    expectBool("chord annotations empty", song.chordAnnotations.isEmpty(), true);
    expectBool("channel index empty", song.eventIndexesByChannel[0].isEmpty(), true);
    expectBool("track index empty", song.eventIndexesByTrack.isEmpty(), true);
}

void testRecordDefaults()
{
    MidiEventRecord record;
    NoteEvent note;

    expectInt("record stream index", record.streamIndex, -1);
    expectInt("record channel", record.channel, -1);
    expectInt("note id", note.id, -1);
    expectInt("note pitch", note.pitch, -1);
}

void testMusicMapDefaults()
{
    TempoChange tempo;
    TimeSignatureChange timeSignature;

    expectInt("default tempo", tempo.microsecondsPerQuarter, 500000);
    expectInt("default numerator", timeSignature.numerator, 4);
    expectInt("default denominator", timeSignature.denominator, 4);
}
}

int main()
{
    testSongDataDefaults();
    testRecordDefaults();
    testMusicMapDefaults();

    if (failures == 0) {
        std::cout << "SongData tests passed\n";
        return 0;
    }
    std::cerr << failures << " song data test(s) failed\n";
    return 1;
}
