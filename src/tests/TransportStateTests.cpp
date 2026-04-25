#include <iostream>

#include "TransportState.h"

namespace {
int failures = 0;

void expectBool(const char *name, bool actual, bool expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

void expectFloat(const char *name, float actual, float expected)
{
    const float diff = actual > expected ? actual - expected : expected - actual;
    if (diff < 0.00001f)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

void expectTick(const char *name, qint64 actual, qint64 expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

void testDefaults()
{
    TransportState state;

    expectTick("default current tick", state.currentTick, 0);
    expectBool("default playing", state.playing, false);
    expectFloat("default speed", state.speed, 1.0f);
    expectBool("default follow playback", state.followPlayback, true);
}

void testRangeChecks()
{
    TransportState state;

    setTransportCurrentTick(state, -10);
    expectTick("negative tick clamps", state.currentTick, 0);
    advanceTransportTick(state, 25);
    expectTick("advance tick", state.currentTick, 25);
    advanceTransportTick(state, -100);
    expectTick("advance clamps", state.currentTick, 0);

    setTransportSpeed(state, 0.01f);
    expectFloat("low speed clamps", state.speed, TransportMinSpeed);
    setTransportSpeed(state, 3.0f);
    expectFloat("high speed clamps", state.speed, TransportMaxSpeed);
}

void testLoop()
{
    TransportState state;

    setTransportLoop(state, 50, 40);
    expectBool("invalid loop disabled", transportLoopEnabled(state), false);
    expectTick("invalid loop end", state.loopEndTick, state.loopStartTick);
    setTransportLoop(state, 10, 40);
    expectBool("valid loop enabled", transportLoopEnabled(state), true);
    setTransportCurrentTick(state, 40);
    expectBool("loop boundary not reached", transportLoopReached(state), false);
    setTransportCurrentTick(state, 41);
    expectBool("loop reached", transportLoopReached(state), true);
}
}

int main()
{
    testDefaults();
    testRangeChecks();
    testLoop();

    if (failures == 0) {
        std::cout << "TransportState tests passed\n";
        return 0;
    }
    std::cerr << failures << " transport state test(s) failed\n";
    return 1;
}
