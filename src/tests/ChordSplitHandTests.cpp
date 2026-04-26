#include <iostream>

#include "Chord.h"

namespace {
int failures = 0;

void expectInt(const char *name, int actual, int expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

CMidiEvent noteOn(int channel, int track, int pitch, int onset = 0)
{
    CMidiEvent event;
    event.noteOnEvent(0, channel, pitch, 80);
    event.setTrack(track);
    event.setAbsoluteTime(onset);
    return event;
}

CSplitHandNote splitNote(int pitch, int onset, int duration)
{
    CSplitHandNote note;
    note.pitch = pitch;
    note.onset = onset;
    note.offset = onset + duration;
    note.velocity = 80;
    note.channel = 0;
    note.track = 0;
    return note;
}

void useCreateChannelsMode(int channel, int rightHandTrack)
{
    CNote::reset();
    CNote::setSplitHands(true);
    CNote::setSplitHandsMode(PB_SPLIT_HANDS_createChannels);
    CNote::setChannelHands(channel, channel);
    CNote::setSplitHandChannel(channel, true);
    CNote::setRightHandTrack(channel, rightHandTrack);
}

void useClusterMode()
{
    CNote::reset();
    CNote::setSplitHands(true);
    CNote::setSplitHandsMode(PB_SPLIT_HANDS_cluster);
    CNote::setClusterMaxHandSpans(MIDI_OCTAVE, MIDI_OCTAVE + 4);
    CNote::setChannelHands(0, 0);
    CNote::setSplitHandChannel(0, true);
}

void testCreateChannelsUsesMidiTracks()
{
    useCreateChannelsMode(0, 1);
    expectInt("right track low pitch",
              CNote::findHand(noteOn(0, 1, 48), 0, PB_PART_both),
              PB_PART_right);
    expectInt("left track high pitch",
              CNote::findHand(noteOn(0, 0, 80), 0, PB_PART_both),
              PB_PART_left);
}

void testCreateChannelsHonorsSelectedHand()
{
    useCreateChannelsMode(0, 1);
    expectInt("right hand hides left track",
              CNote::findHand(noteOn(0, 0, 80), 0, PB_PART_right),
              PB_PART_none);
    expectInt("left hand hides right track",
              CNote::findHand(noteOn(0, 1, 48), 0, PB_PART_left),
              PB_PART_none);
}

void testCreateChannelsReadsSiblingChannelTracksWithoutAudioSplit()
{
    useCreateChannelsMode(0, 1);
    CNote::setRightHandTrack(4, 1);
    CNote::setTrackSplitHandMask(4, PB_TRACK_SPLIT_LEFT_MASK);

    expectInt("sibling split audio flag remains off",
              CNote::splitHandsForChannel(4), false);
    expectInt("sibling channel is not right volume hand",
              CNote::trackSplitChannelHasHand(4, PB_PART_right), false);
    expectInt("sibling channel is left volume hand",
              CNote::trackSplitChannelHasHand(4, PB_PART_left), true);
    expectInt("sibling channel is both volume hand",
              CNote::trackSplitChannelHasHand(4, PB_PART_both), true);
    expectInt("sibling channel right track",
              CNote::findHand(noteOn(4, 1, 48), 0, PB_PART_both),
              PB_PART_right);
    expectInt("sibling channel left track",
              CNote::findHand(noteOn(4, 0, 80), 0, PB_PART_both),
              PB_PART_left);
}

void testClusterRepairsIsolatedWideSpan()
{
    useClusterMode();
    const int beat = CMidiFile::getPulsesPerQuarterNote();
    QVector<CSplitHandNote> notes;
    notes.append(splitNote(72, 0, beat));
    notes.append(splitNote(77, 0, beat));
    notes.append(splitNote(85, 0, beat));
    CNote::assignSplitHands(notes);

    expectInt("cluster wide low note moves left",
              CNote::findHand(noteOn(0, 0, 72), 0, PB_PART_both),
              PB_PART_left);
    expectInt("cluster wide middle stays right",
              CNote::findHand(noteOn(0, 0, 77), 0, PB_PART_both),
              PB_PART_right);
    expectInt("cluster wide top stays right",
              CNote::findHand(noteOn(0, 0, 85), 0, PB_PART_both),
              PB_PART_right);
}

void testClusterAllowsRepeatedSixteenSemitonePattern()
{
    useClusterMode();
    const int beat = CMidiFile::getPulsesPerQuarterNote();
    QVector<CSplitHandNote> notes;
    for (int onset : {0, 8 * beat, 16 * beat})
        for (int pitch : {72, 76, 88})
            notes.append(splitNote(pitch, onset, beat));
    CNote::assignSplitHands(notes);

    expectInt("cluster sixteen-semitone pattern low remains right",
              CNote::findHand(noteOn(0, 0, 72), 0, PB_PART_both),
              PB_PART_right);
    expectInt("cluster sixteen-semitone pattern top remains right",
              CNote::findHand(noteOn(0, 0, 88), 0, PB_PART_both),
              PB_PART_right);
}

void testClusterCustomNormalSpan()
{
    useClusterMode();
    CNote::setClusterMaxHandSpans(MIDI_OCTAVE + 4, MIDI_OCTAVE + 4);
    const int beat = CMidiFile::getPulsesPerQuarterNote();
    QVector<CSplitHandNote> notes;
    notes.append(splitNote(72, 0, beat));
    notes.append(splitNote(77, 0, beat));
    notes.append(splitNote(85, 0, beat));
    CNote::assignSplitHands(notes);

    expectInt("cluster custom normal span keeps low note right",
              CNote::findHand(noteOn(0, 0, 72), 0, PB_PART_both),
              PB_PART_right);
    expectInt("cluster custom normal span keeps top note right",
              CNote::findHand(noteOn(0, 0, 85), 0, PB_PART_both),
              PB_PART_right);
}
}

int main()
{
    testCreateChannelsUsesMidiTracks();
    testCreateChannelsHonorsSelectedHand();
    testCreateChannelsReadsSiblingChannelTracksWithoutAudioSplit();
    testClusterRepairsIsolatedWideSpan();
    testClusterAllowsRepeatedSixteenSemitonePattern();
    testClusterCustomNormalSpan();

    if (failures == 0) {
        std::cout << "Chord split hand tests passed\n";
        return 0;
    }
    std::cerr << failures << " chord split hand test(s) failed\n";
    return 1;
}
