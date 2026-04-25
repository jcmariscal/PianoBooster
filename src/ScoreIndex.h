#ifndef __SCORE_INDEX_H__
#define __SCORE_INDEX_H__

#include "ScoreSlot.h"

QVector<int> visibleScoreSlotIndexes(const QVector<ScoreSlot>& scoreSlots,
                                     qint64 originTick,
                                     qint64 durationTicks);

#endif
