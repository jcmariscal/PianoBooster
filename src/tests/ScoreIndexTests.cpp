#include <iostream>

#include "ScoreIndex.h"

namespace {
int failures = 0;

void expectInt(const char *name, int actual, int expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

ScoreSlot slotAt(int id, qint64 tick, qint64 duration, qint64 leftEdge)
{
    ScoreSlot slot;
    slot.id = id;
    slot.absoluteTick = tick;
    slot.durationTicks = duration;
    slot.leftEdgeTicks = leftEdge;
    return slot;
}

void testInsideWindow()
{
    QVector<ScoreSlot> scoreSlots;
    scoreSlots.append(slotAt(0, 10, 0, 10));
    scoreSlots.append(slotAt(1, 20, 0, 20));
    scoreSlots.append(slotAt(2, 30, 0, 30));

    const QVector<int> indexes = visibleScoreSlotIndexes(scoreSlots, 15, 10);
    expectInt("inside count", indexes.size(), 1);
    expectInt("inside index", indexes[0], 1);
}

void testDurationOverlapBeforeWindow()
{
    QVector<ScoreSlot> scoreSlots;
    scoreSlots.append(slotAt(0, 10, 10, 10));
    scoreSlots.append(slotAt(1, 30, 0, 30));

    const QVector<int> indexes = visibleScoreSlotIndexes(scoreSlots, 15, 5);
    expectInt("duration overlap count", indexes.size(), 1);
    expectInt("duration overlap index", indexes[0], 0);
}

void testLeftEdgeOverlap()
{
    QVector<ScoreSlot> scoreSlots;
    scoreSlots.append(slotAt(0, 20, 0, 12));
    scoreSlots.append(slotAt(1, 40, 0, 40));

    const QVector<int> indexes = visibleScoreSlotIndexes(scoreSlots, 15, 1);
    expectInt("left edge count", indexes.size(), 1);
    expectInt("left edge index", indexes[0], 0);
}
}

int main()
{
    testInsideWindow();
    testDurationOverlapBeforeWindow();
    testLeftEdgeOverlap();

    if (failures == 0) {
        std::cout << "ScoreIndex tests passed\n";
        return 0;
    }
    std::cerr << failures << " score index test(s) failed\n";
    return 1;
}
