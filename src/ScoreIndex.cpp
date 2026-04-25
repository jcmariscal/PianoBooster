#include "ScoreIndex.h"

#include <algorithm>

namespace {
qint64 validTick(qint64 tick)
{
    return tick > 0 ? tick : 0;
}

qint64 slotStart(const ScoreSlot& slot)
{
    return std::min(slot.leftEdgeTicks, slot.absoluteTick);
}

qint64 slotEnd(const ScoreSlot& slot)
{
    const qint64 endTick = slot.absoluteTick + std::max<qint64>(slot.durationTicks, 0);
    return std::max(slot.absoluteTick, endTick);
}

bool slotOverlapsWindow(const ScoreSlot& slot, qint64 startTick, qint64 endTick)
{
    return slotStart(slot) <= endTick && slotEnd(slot) >= startTick;
}

int firstCandidateIndex(const QVector<ScoreSlot>& scoreSlots, qint64 startTick)
{
    auto it = std::lower_bound(scoreSlots.begin(), scoreSlots.end(), startTick,
                               [](const ScoreSlot& slot, qint64 tick) {
        return slot.absoluteTick < tick;
    });
    int index = static_cast<int>(it - scoreSlots.begin());
    while (index > 0 && slotEnd(scoreSlots[index - 1]) >= startTick)
        index--;
    return index;
}
}

QVector<int> visibleScoreSlotIndexes(const QVector<ScoreSlot>& scoreSlots,
                                     qint64 originTick,
                                     qint64 durationTicks)
{
    const qint64 startTick = validTick(originTick);
    const qint64 endTick = startTick + validTick(durationTicks);
    QVector<int> indexes;

    for (int i = firstCandidateIndex(scoreSlots, startTick); i < scoreSlots.size(); i++)
    {
        if (slotStart(scoreSlots[i]) > endTick)
            break;
        if (slotOverlapsWindow(scoreSlots[i], startTick, endTick))
            indexes.append(i);
    }
    return indexes;
}
