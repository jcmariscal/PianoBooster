#include <iostream>

#include "NotationBuilder.h"

int CMidiFile::m_ppqn = DEFAULT_PPQN;

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

MidiEventRecord noteRecord(qint64 tick, int channel, int pitch, int duration, int track = 0)
{
    MidiEventRecord record;
    record.absoluteTick = tick;
    record.channel = channel;
    record.track = track;
    record.event.noteOnEvent(0, channel, pitch, 64);
    record.event.setAbsoluteTime(static_cast<int>(tick));
    record.event.setDuration(duration);
    record.event.setTrack(track);
    return record;
}

MidiEventRecord eofRecord(qint64 tick)
{
    MidiEventRecord record;
    record.absoluteTick = tick;
    record.event.setType(MIDI_PB_EOF);
    return record;
}

NoteEvent noteEvent(int id, qint64 tick, int channel, int pitch, int track = 0)
{
    NoteEvent note;
    note.id = id;
    note.startTick = tick;
    note.endTick = tick + 48;
    note.pitch = pitch;
    note.channel = channel;
    note.track = track;
    return note;
}

SongData singleNoteSong(qint64 tick, int channel)
{
    SongData song;
    song.events.append(noteRecord(tick, channel, 60, 48));
    song.events.append(eofRecord(tick + 96));
    song.notes.append(noteEvent(7, tick, channel, 60));
    song.durationTicks = tick + 96;
    return song;
}

const ScoreSlot* firstNoteSlot(const QVector<ScoreSlot>& scoreSlots)
{
    for (int i = 0; i < scoreSlots.size(); i++)
        for (int j = 0; j < scoreSlots[i].symbols.size(); j++)
            if (scoreSlots[i].symbols[j].type >= PB_SYMBOL_noteHead)
                return &scoreSlots[i];
    return nullptr;
}

void testSingleNote()
{
    const QVector<ScoreSlot> scoreSlots = buildNotationSlots(singleNoteSong(0, 0), 0);
    const ScoreSlot* slot = firstNoteSlot(scoreSlots);

    expectBool("single note slot exists", slot != nullptr, true);
    if (slot == nullptr)
        return;
    expectInt("single note tick", static_cast<int>(slot->absoluteTick), 0);
    expectInt("single note id", slot->symbols[0].noteId, 7);
    expectInt("single note pitch", slot->symbols[0].midiNote, 60);
}

void testBeatMarkerBeforeLaterNote()
{
    const QVector<ScoreSlot> scoreSlots = buildNotationSlots(singleNoteSong(120, 0), 0);

    expectBool("beat marker present", scoreSlots.size() > 0, true);
    if (scoreSlots.isEmpty())
        return;
    expectInt("first slot is beat marker", scoreSlots[0].symbols[0].type,
              PB_SYMBOL_beatMarker);
}

void testChannelFiltering()
{
    const QVector<ScoreSlot> wrongChannel = buildNotationSlots(singleNoteSong(0, 1), 0);
    const QVector<ScoreSlot> rightChannel = buildNotationSlots(singleNoteSong(0, 1), 1);

    expectBool("wrong channel has no note", firstNoteSlot(wrongChannel) == nullptr, true);
    expectBool("right channel has note", firstNoteSlot(rightChannel) != nullptr, true);
}

void testTrackSplitIncludesSiblingBassChannel()
{
    CNote::reset();
    CNote::setSplitHands(true);
    CNote::setSplitHandsMode(PB_SPLIT_HANDS_createChannels);
    CNote::setChannelHands(0, 0);
    CNote::setSplitHandChannel(0, true);
    CNote::setRightHandTrack(0, 0);
    CNote::setRightHandTrack(4, 0);

    SongData song;
    song.events.append(noteRecord(0, 0, 76, 48, 0));
    song.events.append(noteRecord(0, 0, 52, 48, 1));
    song.events.append(noteRecord(0, 4, 43, 48, 1));
    song.events.append(eofRecord(96));
    song.notes.append(noteEvent(1, 0, 0, 76, 0));
    song.notes.append(noteEvent(2, 0, 0, 52, 1));
    song.notes.append(noteEvent(3, 0, 4, 43, 1));
    song.durationTicks = 96;

    const QVector<ScoreSlot> scoreSlots = buildNotationSlots(song, 0);
    const ScoreSlot* slot = firstNoteSlot(scoreSlots);
    expectBool("track split slot exists", slot != nullptr, true);
    if (slot == nullptr)
    {
        CNote::reset();
        return;
    }
    expectInt("track split note count", slot->symbols.size(), 3);
    expectInt("track split high track hand", slot->symbols[2].hand, PB_PART_right);
    expectInt("track split bass sibling channel hand", slot->symbols[0].hand, PB_PART_left);
    CNote::reset();
}
}

int main()
{
    CNote::reset();
    testSingleNote();
    testBeatMarkerBeforeLaterNote();
    testChannelFiltering();
    testTrackSplitIncludesSiblingBassChannel();

    if (failures == 0) {
        std::cout << "NotationBuilder tests passed\n";
        return 0;
    }
    std::cerr << failures << " notation builder test(s) failed\n";
    return 1;
}
