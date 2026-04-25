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
}

int main()
{
    testCreateChannelsUsesMidiTracks();
    testCreateChannelsHonorsSelectedHand();

    if (failures == 0) {
        std::cout << "Chord split hand tests passed\n";
        return 0;
    }
    std::cerr << failures << " chord split hand test(s) failed\n";
    return 1;
}
