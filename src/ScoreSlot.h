#ifndef __SCORE_SLOT_H__
#define __SCORE_SLOT_H__

#include <QVector>

#include "Symbol.h"

struct ScoreSlotSymbol
{
    musicalSymbol_t type = PB_SYMBOL_none;
    whichPart_t hand = PB_PART_none;
    int midiNote = 0;
    int midiDuration = 0;
    accidentalModifer_t accidentalModifier = PB_ACCIDENTAL_MODIFER_noChange;
    int noteIndex = 0;
    int noteTotal = 0;
    int noteId = -1;
};

struct ScoreSlot
{
    int id = -1;
    qint64 absoluteTick = 0;
    qint64 durationTicks = 0;
    qint64 leftEdgeTicks = 0;
    int av8Left = 0;
    QVector<ScoreSlotSymbol> symbols;
};

#endif
