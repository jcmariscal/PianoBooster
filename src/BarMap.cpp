#include "BarMap.h"

#include <algorithm>

namespace {
int safePpqn(int ppqn)
{
    return ppqn > 0 ? ppqn : SongDataDefaultPpqn;
}

int safeValue(int value, int fallback)
{
    return value > 0 ? value : fallback;
}

qint64 beatLengthTicks(int ppqn, int denominator)
{
    const qint64 ticks = static_cast<qint64>(safePpqn(ppqn)) * 4 / safeValue(denominator, 4);
    return ticks > 0 ? ticks : 1;
}

QVector<TimeSignatureChange> sortedTimeSignatures(QVector<TimeSignatureChange> signatures)
{
    if (signatures.isEmpty())
        signatures.append(TimeSignatureChange());
    std::sort(signatures.begin(), signatures.end(),
              [](const TimeSignatureChange& a, const TimeSignatureChange& b) {
        return a.tick < b.tick;
    });
    if (signatures.first().tick > 0)
        signatures.prepend(TimeSignatureChange());
    return signatures;
}

void appendBar(BarMap& map, qint64 tick, qint64 beatLength, int beats, int denominator)
{
    if (!map.barStarts.isEmpty() && map.barStarts.last() == tick)
    {
        map.beatLengths.last() = beatLength;
        map.beatsPerBar.last() = beats;
        map.denominators.last() = denominator;
        return;
    }
    map.barStarts.append(tick);
    map.beatLengths.append(beatLength);
    map.beatsPerBar.append(beats);
    map.denominators.append(denominator);
}

void appendBarsUntil(BarMap& map, qint64& tick, qint64 limit,
                     qint64 beatLength, int beats, int denominator)
{
    const qint64 barLength = beatLength * beats;
    while (tick <= limit)
    {
        appendBar(map, tick, beatLength, beats, denominator);
        if (barLength <= 0 || tick + barLength > limit)
            return;
        tick += barLength;
    }
}
}

BarMap buildBarMap(int ppqn, qint64 durationTicks,
                   const QVector<TimeSignatureChange>& timeSignatures)
{
    BarMap map;
    map.durationTicks = durationTicks > 0 ? durationTicks : 0;
    QVector<TimeSignatureChange> signatures = sortedTimeSignatures(timeSignatures);
    qint64 tick = 0;

    for (int i = 0; i < signatures.size() && tick <= map.durationTicks; i++)
    {
        const TimeSignatureChange signature = signatures[i];
        const qint64 start = signature.tick > 0 ? signature.tick : 0;
        if (start > map.durationTicks)
            break;
        if (start > tick)
            tick = start;
        qint64 limit = map.durationTicks;
        if (i + 1 < signatures.size() && signatures[i + 1].tick > start)
            limit = std::min(map.durationTicks, signatures[i + 1].tick);
        const int denominator = safeValue(signature.denominator, 4);
        const qint64 beatLength = beatLengthTicks(ppqn, denominator);
        appendBarsUntil(map, tick, limit, beatLength,
                        safeValue(signature.numerator, 4), denominator);
        if (i + 1 < signatures.size() && signatures[i + 1].tick > tick)
            tick = signatures[i + 1].tick;
    }
    if (map.barStarts.isEmpty())
        appendBar(map, 0, beatLengthTicks(ppqn, 4), 4, 4);
    return map;
}

int barAtTick(const BarMap& map, qint64 tick)
{
    if (map.barStarts.isEmpty())
        return 0;
    if (tick < 0)
        tick = 0;
    const auto it = std::upper_bound(map.barStarts.begin(), map.barStarts.end(), tick);
    if (it == map.barStarts.begin())
        return 0;
    return static_cast<int>((it - map.barStarts.begin()) - 1);
}

int beatAtTick(const BarMap& map, qint64 tick)
{
    const int bar = barAtTick(map, tick);
    if (bar < 0 || bar >= map.barStarts.size())
        return 0;
    const qint64 beatLength = map.beatLengths[bar];
    if (beatLength <= 0)
        return 0;
    const qint64 offset = tick - map.barStarts[bar];
    if (offset <= 0)
        return 0;
    const int beat = static_cast<int>(offset / beatLength);
    return beat < map.beatsPerBar[bar] ? beat : map.beatsPerBar[bar] - 1;
}

qint64 tickAtBar(const BarMap& map, int bar)
{
    if (map.barStarts.isEmpty() || bar <= 0)
        return 0;
    if (bar >= map.barStarts.size())
        return map.durationTicks;
    return map.barStarts[bar];
}

qint64 tickAtBarPosition(const BarMap& map, double barPosition)
{
    if (map.barStarts.isEmpty() || barPosition <= 0.0)
        return 0;
    const int bar = static_cast<int>(barPosition);
    if (bar >= map.barStarts.size())
        return map.durationTicks;
    const double fraction = barPosition - static_cast<double>(bar);
    const qint64 offset = static_cast<qint64>(
                fraction * static_cast<double>(map.beatLengths[bar] * map.denominators[bar]));
    const qint64 tick = map.barStarts[bar] + offset;
    return tick < map.durationTicks ? tick : map.durationTicks;
}

double barPositionAtTick(const BarMap& map, qint64 tick)
{
    const int bar = barAtTick(map, tick);
    if (bar < 0 || bar >= map.barStarts.size())
        return 0.0;
    const qint64 beatLength = map.beatLengths[bar];
    const int denominator = map.denominators[bar];
    if (beatLength <= 0 || denominator <= 0)
        return static_cast<double>(bar);
    const qint64 offset = tick - map.barStarts[bar];
    if (offset <= 0)
        return static_cast<double>(bar);
    return static_cast<double>(bar) +
            static_cast<double>(offset) / static_cast<double>(beatLength * denominator);
}
