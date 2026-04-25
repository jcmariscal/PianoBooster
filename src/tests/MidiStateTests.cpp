#include <iostream>

#include "MidiState.h"

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

MidiEventRecord programRecord(qint64 tick, int channel, int program)
{
    MidiEventRecord record;
    record.absoluteTick = tick;
    record.channel = channel;
    record.event.programChangeEvent(9, channel, program);
    return record;
}

MidiEventRecord controllerRecord(qint64 tick, int channel, int controller, int value)
{
    MidiEventRecord record;
    record.absoluteTick = tick;
    record.channel = channel;
    record.event.controlChangeEvent(9, channel, controller, value);
    return record;
}

void testSnapshotBeforeTick()
{
    QVector<MidiEventRecord> events;
    events.append(programRecord(10, 1, 5));
    events.append(controllerRecord(12, 1, MIDI_MAIN_VOLUME, 80));
    events.append(programRecord(30, 1, 8));

    MidiStateSnapshot state = buildMidiStateSnapshot(events, 20);
    expectBool("program present", state.channels[1].hasProgram, true);
    expectInt("program value", state.channels[1].program.programme(), 5);
    expectBool("volume present", state.channels[1].hasController[MIDI_MAIN_VOLUME], true);
    expectInt("volume delta", state.channels[1].controllers[MIDI_MAIN_VOLUME].deltaTime(), 0);
}

void testResetControllers()
{
    QVector<MidiEventRecord> events;
    events.append(controllerRecord(10, 2, MIDI_MAIN_VOLUME, 80));
    events.append(controllerRecord(12, 2, MIDI_RESET_ALL_CONTROLLERS, 0));

    MidiStateSnapshot state = buildMidiStateSnapshot(events, 20);
    expectBool("volume cleared", state.channels[2].hasController[MIDI_MAIN_VOLUME], false);
}

void testTransientControllersIgnored()
{
    QVector<MidiEventRecord> events;
    events.append(controllerRecord(10, 3, MIDI_ALL_NOTES_OFF, 0));

    MidiStateSnapshot state = buildMidiStateSnapshot(events, 20);
    expectBool("all notes off ignored", state.channels[3].hasController[MIDI_ALL_NOTES_OFF], false);
}
}

int main()
{
    testSnapshotBeforeTick();
    testResetControllers();
    testTransientControllersIgnored();

    if (failures == 0) {
        std::cout << "MidiState tests passed\n";
        return 0;
    }
    std::cerr << failures << " midi state test(s) failed\n";
    return 1;
}
