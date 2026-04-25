#include <cmath>
#include <iostream>

#include "Bar.h"

// CBar only needs PPQN; keep this test independent of MIDI file parsing.
int CMidiFile::m_ppqn = DEFAULT_PPQN;

namespace {
int failures = 0;

void expectInt(const char *name, int actual, int expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

void expectNear(const char *name, double actual, double expected)
{
    if (std::fabs(actual - expected) < 0.000001)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

qint64 beatTicks(int beats)
{
    return static_cast<qint64>(DEFAULT_PPQN) * SPEED_ADJUST_FACTOR * beats + beats;
}

void testDefaultTimeSignature()
{
    CBar bar;
    int top = 0;
    int bottom = 0;

    bar.getTimeSig(&top, &bottom);

    expectInt("default time signature top", top, 4);
    expectInt("default time signature bottom", bottom, 4);
    expectInt("default bar number", bar.getBarNumber(), 0);
    expectNear("default bar position", bar.getCurrentBarPos(), 0.0);
}

void testFourFourBarAdvance()
{
    CBar bar;

    bar.addDeltaTime(beatTicks(4));

    expectInt("4/4 bar number", bar.getBarNumber(), 1);
    expectInt("4/4 new bar event", bar.readEventBits() & EVENT_BITS_newBarNumber,
              EVENT_BITS_newBarNumber);
    expectInt("4/4 event bits clear", bar.readEventBits(), 0);
}

void testThreeFourBarAdvance()
{
    CBar bar;

    bar.setTimeSig(0, 0);
    bar.setTimeSig(3, 4);
    bar.addDeltaTime(beatTicks(3));

    expectInt("3/4 bar number", bar.getBarNumber(), 1);
    expectInt("3/4 beat length", static_cast<int>(bar.getBeatLength()), DEFAULT_PPQN);
    expectInt("3/4 bar length", static_cast<int>(bar.getBarLength()), DEFAULT_PPQN * 3);
}
}

int main()
{
    testDefaultTimeSignature();
    testFourFourBarAdvance();
    testThreeFourBarAdvance();

    if (failures == 0) {
        std::cout << "Bar tests passed\n";
        return 0;
    }
    std::cerr << failures << " bar test(s) failed\n";
    return 1;
}
