#include <iostream>

#include "BarMap.h"
#include "Bar.h"

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

void expectTick(const char *name, qint64 actual, qint64 expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

void expectNear(const char *name, double actual, double expected)
{
    const double diff = actual > expected ? actual - expected : expected - actual;
    if (diff < 0.00001)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

TimeSignatureChange signature(qint64 tick, int numerator, int denominator)
{
    TimeSignatureChange change;
    change.tick = tick;
    change.numerator = numerator;
    change.denominator = denominator;
    return change;
}

void testFourFour()
{
    const BarMap map = buildBarMap(96, 96 * 4, QVector<TimeSignatureChange>());

    expectTick("4/4 first bar", tickAtBar(map, 0), 0);
    expectTick("4/4 second bar", tickAtBar(map, 1), 96 * 4);
    expectTick("4/4 half bar", tickAtBarPosition(map, 0.5), 96 * 2);
    expectInt("4/4 bar before boundary", barAtTick(map, 96 * 4 - 1), 0);
    expectInt("4/4 bar at boundary", barAtTick(map, 96 * 4), 1);
    expectInt("4/4 beat", beatAtTick(map, 96 * 2), 2);
    expectNear("4/4 position", barPositionAtTick(map, 96 * 2), 0.5);
}

void testThreeFour()
{
    QVector<TimeSignatureChange> signatures;
    signatures.append(signature(0, 3, 4));
    const BarMap map = buildBarMap(96, 96 * 3, signatures);

    expectTick("3/4 second bar", tickAtBar(map, 1), 96 * 3);
    expectInt("3/4 bar before boundary", barAtTick(map, 96 * 3 - 1), 0);
    expectInt("3/4 beat", beatAtTick(map, 96 * 2), 2);
    expectNear("3/4 position", barPositionAtTick(map, 96 * 2), 0.5);
}

void testSignatureChange()
{
    QVector<TimeSignatureChange> signatures;
    signatures.append(signature(0, 4, 4));
    signatures.append(signature(96 * 4, 3, 4));
    const BarMap map = buildBarMap(96, 96 * 7, signatures);

    expectTick("changed second bar", tickAtBar(map, 1), 96 * 4);
    expectTick("changed third bar", tickAtBar(map, 2), 96 * 7);
    expectInt("changed bar after switch", barAtTick(map, 96 * 5), 1);
    expectInt("changed beat after switch", beatAtTick(map, 96 * 6), 2);
}

qint64 conductorTicks(qint64 rawTicks)
{
    return rawTicks * SPEED_ADJUST_FACTOR + 1;
}

void testAgainstCBar()
{
    CBar bar;
    QVector<TimeSignatureChange> signatures;
    signatures.append(signature(0, 4, 4));
    const BarMap map = buildBarMap(96, 96 * 4, signatures);

    bar.addDeltaTime(conductorTicks(96 * 2));
    expectInt("CBar half-bar number", barAtTick(map, 96 * 2), bar.getBarNumber());
    expectNear("CBar half-bar position", barPositionAtTick(map, 96 * 2),
               bar.getCurrentBarPos());
    bar.addDeltaTime(conductorTicks(96 * 2));
    expectInt("CBar next bar number", barAtTick(map, 96 * 4), bar.getBarNumber());
}
}

int main()
{
    testFourFour();
    testThreeFour();
    testSignatureChange();
    testAgainstCBar();

    if (failures == 0) {
        std::cout << "BarMap tests passed\n";
        return 0;
    }
    std::cerr << failures << " bar map test(s) failed\n";
    return 1;
}
