#include "NoteIndex.h"

#include <algorithm>

namespace {
qint64 validTick(qint64 tick)
{
    return tick > 0 ? tick : 0;
}

qint64 noteEnd(const NoteEvent& note)
{
    return std::max(note.startTick, note.endTick);
}

bool noteOverlapsWindow(const NoteEvent& note, qint64 startTick, qint64 endTick)
{
    return note.startTick <= endTick && noteEnd(note) >= startTick;
}

int firstNoteAfterWindow(const QVector<NoteEvent>& notes, qint64 endTick)
{
    auto it = std::upper_bound(notes.begin(), notes.end(), endTick,
                               [](qint64 tick, const NoteEvent& note) {
        return tick < note.startTick;
    });
    return static_cast<int>(it - notes.begin());
}
}

QVector<int> visibleNoteIndexes(const QVector<NoteEvent>& notes,
                                qint64 originTick,
                                qint64 durationTicks)
{
    const qint64 startTick = validTick(originTick);
    const qint64 endTick = startTick + validTick(durationTicks);
    const int endIndex = firstNoteAfterWindow(notes, endTick);
    QVector<int> indexes;

    for (int i = 0; i < endIndex; i++)
        if (noteOverlapsWindow(notes[i], startTick, endTick))
            indexes.append(i);
    return indexes;
}
