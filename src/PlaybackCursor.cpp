#include "PlaybackCursor.h"

#include <algorithm>
#include <limits>

namespace {
qint64 validTick(qint64 tick)
{
    return tick > 0 ? tick : 0;
}

int cursorDelta(qint64 fromTick, qint64 toTick)
{
    qint64 delta = toTick - fromTick;
    if (delta < 0)
        delta = 0;
    if (delta > std::numeric_limits<int>::max())
        return std::numeric_limits<int>::max();
    return static_cast<int>(delta);
}
}

void setPlaybackCursorEvents(PlaybackCursor& cursor,
                             const QVector<MidiEventRecord>* events)
{
    cursor.events = events;
    seekPlaybackCursor(cursor, 0);
}

void seekPlaybackCursor(PlaybackCursor& cursor, qint64 tick)
{
    cursor.tick = validTick(tick);
    cursor.index = 0;
    if (cursor.events == nullptr)
        return;

    auto it = std::lower_bound(cursor.events->begin(), cursor.events->end(), cursor.tick,
                               [](const MidiEventRecord& record, qint64 value) {
        return record.absoluteTick < value;
    });
    cursor.index = static_cast<int>(it - cursor.events->begin());
}

bool readPlaybackCursorEvent(PlaybackCursor& cursor, CMidiEvent& event)
{
    if (cursor.events == nullptr || cursor.index < 0 ||
            cursor.index >= cursor.events->size())
        return false;

    const MidiEventRecord record = cursor.events->at(cursor.index++);
    event = record.event;
    event.setDeltaTime(cursorDelta(cursor.tick, record.absoluteTick));
    cursor.tick = record.absoluteTick;
    return true;
}

int playbackCursorIndex(const PlaybackCursor& cursor)
{
    return cursor.index;
}
