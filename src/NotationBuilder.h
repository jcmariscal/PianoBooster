#ifndef __NOTATION_BUILDER_H__
#define __NOTATION_BUILDER_H__

#include "ScoreSlot.h"
#include "SongData.h"

QVector<ScoreSlot> buildNotationSlots(const SongData& song, int displayChannel);

#endif
