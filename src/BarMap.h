#ifndef PIANOBOOSTER_BAR_MAP_H
#define PIANOBOOSTER_BAR_MAP_H

#include <QVector>
#include <QtGlobal>

#include "SongData.h"

struct BarMap
{
    qint64 durationTicks = 0;
    QVector<qint64> barStarts;
    QVector<qint64> beatLengths;
    QVector<int> beatsPerBar;
    QVector<int> denominators;
};

BarMap buildBarMap(int ppqn, qint64 durationTicks,
                   const QVector<TimeSignatureChange>& timeSignatures);
int barAtTick(const BarMap& map, qint64 tick);
int beatAtTick(const BarMap& map, qint64 tick);
qint64 tickAtBar(const BarMap& map, int bar);
qint64 tickAtBarPosition(const BarMap& map, double barPosition);
double barPositionAtTick(const BarMap& map, qint64 tick);

#endif
