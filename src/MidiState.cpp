#include "MidiState.h"

namespace {
bool validStateChannel(int channel)
{
    return channel >= 0 && channel < MAX_MIDI_CHANNELS;
}

bool validController(int controller)
{
    return controller >= 0 && controller < MidiControllerCount;
}

bool transientController(int controller)
{
    return controller == MIDI_ALL_SOUND_OFF || controller == MIDI_ALL_NOTES_OFF;
}

void resetChannelControllers(MidiChannelState& channel)
{
    for (int i = 0; i < MidiControllerCount; i++)
        channel.hasController[i] = false;
}

CMidiEvent stateEvent(CMidiEvent event)
{
    event.setDeltaTime(0);
    return event;
}
}

void resetMidiStateSnapshot(MidiStateSnapshot& state)
{
    for (int channel = 0; channel < MAX_MIDI_CHANNELS; channel++)
    {
        state.channels[channel].hasProgram = false;
        resetChannelControllers(state.channels[channel]);
    }
}

void rememberMidiStateEvent(MidiStateSnapshot& state, const MidiEventRecord& record)
{
    if (!validStateChannel(record.channel))
        return;
    MidiChannelState& channel = state.channels[record.channel];
    if (record.event.type() == MIDI_PROGRAM_CHANGE)
    {
        channel.program = stateEvent(record.event);
        channel.hasProgram = true;
    }
    else if (record.event.type() == MIDI_CONTROL_CHANGE)
    {
        const int controller = record.event.data1();
        if (!validController(controller) || transientController(controller))
            return;
        if (controller == MIDI_RESET_ALL_CONTROLLERS)
            resetChannelControllers(channel);
        else
        {
            channel.controllers[controller] = stateEvent(record.event);
            channel.hasController[controller] = true;
        }
    }
}

MidiStateSnapshot buildMidiStateSnapshot(const QVector<MidiEventRecord>& events,
                                         qint64 tick)
{
    MidiStateSnapshot state;
    resetMidiStateSnapshot(state);
    for (int i = 0; i < events.size(); i++)
    {
        if (events[i].absoluteTick > tick)
            break;
        rememberMidiStateEvent(state, events[i]);
    }
    return state;
}
