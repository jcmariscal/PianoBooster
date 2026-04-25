#ifndef __MIDI_STATE_H__
#define __MIDI_STATE_H__

#include "SongData.h"

constexpr int MidiControllerCount = 128;
constexpr int MidiBankSelectMsb = 0;
constexpr int MidiBankSelectLsb = 32;

struct MidiChannelState
{
    bool hasProgram;
    CMidiEvent program;
    bool hasController[MidiControllerCount];
    CMidiEvent controllers[MidiControllerCount];
};

struct MidiStateSnapshot
{
    MidiChannelState channels[MAX_MIDI_CHANNELS];
};

void resetMidiStateSnapshot(MidiStateSnapshot& state);
void rememberMidiStateEvent(MidiStateSnapshot& state, const MidiEventRecord& record);
MidiStateSnapshot buildMidiStateSnapshot(const QVector<MidiEventRecord>& events,
                                         qint64 tick);

#endif
