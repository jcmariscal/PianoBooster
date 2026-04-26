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

CMidiEvent noteOn(int channel, int track, int pitch)
{
    CMidiEvent event;
    event.noteOnEvent(0, channel, pitch, 80);
    event.setTrack(track);
    return event;
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
}

int main()
{
    testCreateChannelsUsesMidiTracks();
    testCreateChannelsHonorsSelectedHand();
    testCreateChannelsReadsSiblingChannelTracksWithoutAudioSplit();

    if (failures == 0) {
        std::cout << "Chord split hand tests passed\n";
        return 0;
    }
    std::cerr << failures << " chord split hand test(s) failed\n";
    return 1;
}
