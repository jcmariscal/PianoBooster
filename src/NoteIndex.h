#ifndef __NOTE_INDEX_H__
#define __NOTE_INDEX_H__

#include "SongData.h"

QVector<int> visibleNoteIndexes(const QVector<NoteEvent>& notes,
                                qint64 originTick,
                                qint64 durationTicks);

#endif
