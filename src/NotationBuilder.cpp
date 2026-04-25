#include "NotationBuilder.h"

#include "BarMap.h"
#include "Notation.h"

#include <QHash>
#include <algorithm>
#include <limits>

namespace {
int safeDelta(qint64 fromTick, qint64 toTick)
{
    qint64 delta = toTick - fromTick;
    if (delta < 0)
        delta = 0;
    if (delta > std::numeric_limits<int>::max())
        return std::numeric_limits<int>::max();
    return static_cast<int>(delta);
}

CMidiEvent notationEvent(const MidiEventRecord& record, qint64 previousTick)
{
    CMidiEvent event = record.event;
    event.setDeltaTime(safeDelta(previousTick, record.absoluteTick));
    return event;
}

QHash<qint64, QVector<int>> noteIndexesByStartTick(const QVector<NoteEvent>& notes)
{
    QHash<qint64, QVector<int>> indexes;
    for (int i = 0; i < notes.size(); i++)
        indexes[notes[i].startTick].append(i);
    return indexes;
}

int matchingNoteId(const SongData& song, const QHash<qint64, QVector<int>>& indexes,
                   qint64 tick, int pitch)
{
    const QVector<int> matches = indexes.value(tick);
    for (int i = 0; i < matches.size(); i++)
    {
        const NoteEvent note = song.notes[matches[i]];
        if (note.pitch == pitch)
            return note.id;
    }
    return -1;
}

ScoreSlotSymbol scoreSlotSymbol(CSymbol symbol, int noteId)
{
    ScoreSlotSymbol result;
    result.type = symbol.getType();
    result.hand = symbol.getHand();
    result.midiNote = symbol.getNote();
    result.midiDuration = symbol.getMidiDuration();
    result.accidentalModifier = symbol.getAccidentalModifer();
    result.noteIndex = symbol.getNoteIndex();
    result.noteTotal = symbol.getNoteTotal();
    result.noteId = noteId;
    return result;
}

qint64 slotDuration(const QVector<ScoreSlotSymbol>& symbols)
{
    qint64 duration = 0;
    for (int i = 0; i < symbols.size(); i++)
        duration = std::max(duration, static_cast<qint64>(symbols[i].midiDuration));
    return duration;
}

ScoreSlot scoreSlot(const SongData& song, const QHash<qint64, QVector<int>>& indexes,
                    CSlot slot, int id, qint64 absoluteTick)
{
    ScoreSlot result;
    result.id = id;
    result.absoluteTick = absoluteTick;
    result.leftEdgeTicks = absoluteTick + slot.getLeftSideDeltaTime() - slot.getDeltaTime();
    result.av8Left = slot.getAv8Left();
    for (int i = 0; i < slot.length(); i++)
    {
        CSymbol symbol = slot.getSymbol(i);
        const int noteId = matchingNoteId(song, indexes, absoluteTick, symbol.getNote());
        result.symbols.append(scoreSlotSymbol(symbol, noteId));
    }
    result.durationTicks = slotDuration(result.symbols);
    return result;
}

bool isEndSlot(CSlot slot)
{
    return slot.length() > 0 && slot.getSymbolType(0) == PB_SYMBOL_theEndMarker;
}

qint64 adjustedTicks(int ppqn, int defaultTicks)
{
    const int safePpqn = ppqn > 0 ? ppqn : SongDataDefaultPpqn;
    return static_cast<qint64>(defaultTicks) * safePpqn / DEFAULT_PPQN;
}

ScoreSlot markerSlot(qint64 tick, qint64 earlyOffset, musicalSymbol_t type)
{
    ScoreSlot slot;
    ScoreSlotSymbol symbol;
    symbol.type = type;
    symbol.hand = PB_PART_both;
    slot.absoluteTick = tick > earlyOffset ? tick - earlyOffset : 0;
    slot.leftEdgeTicks = slot.absoluteTick;
    slot.symbols.append(symbol);
    return slot;
}

void appendMarker(QVector<ScoreSlot>& scoreSlots, qint64 tick,
                  qint64 earlyOffset, musicalSymbol_t type)
{
    if (tick < 0)
        return;
    scoreSlots.append(markerSlot(tick, earlyOffset, type));
}

void appendBeatMarkers(QVector<ScoreSlot>& scoreSlots, const BarMap& map, int ppqn)
{
    const qint64 earlyOffset = adjustedTicks(ppqn, BEAT_MARKER_OFFSET);
    const qint64 barGap = adjustedTicks(ppqn, 30);
    for (int bar = 0; bar < map.barStarts.size(); bar++)
    {
        const qint64 barStart = map.barStarts[bar];
        if (bar > 0)
        {
            appendMarker(scoreSlots, barStart - barGap, earlyOffset, PB_SYMBOL_barLine);
            appendMarker(scoreSlots, barStart, earlyOffset, PB_SYMBOL_barMarker);
        }
        for (int beat = 1; beat < map.beatsPerBar[bar]; beat++)
        {
            const qint64 tick = barStart + map.beatLengths[bar] * beat;
            if (tick > map.durationTicks)
                break;
            appendMarker(scoreSlots, tick, earlyOffset, PB_SYMBOL_beatMarker);
        }
    }
}

void finishSlotOrder(QVector<ScoreSlot>& scoreSlots)
{
    std::stable_sort(scoreSlots.begin(), scoreSlots.end(),
                     [](const ScoreSlot& a, const ScoreSlot& b) {
        return a.absoluteTick < b.absoluteTick;
    });
    for (int i = 0; i < scoreSlots.size(); i++)
        scoreSlots[i].id = i;
}
}

QVector<ScoreSlot> buildNotationSlots(const SongData& song, int displayChannel)
{
    CStavePos::setKeySignature(NOT_USED, 0);
    const BarMap barMap = buildBarMap(song.ppqn, song.durationTicks, song.timeSignatures);
    CNotation notation;
    notation.setBarMap(&barMap);
    notation.setChannel(displayChannel);
    qint64 previousTick = 0;

    for (int i = 0; i < song.events.size(); i++)
    {
        notation.appendMidiEvent(notationEvent(song.events[i], previousTick));
        previousTick = song.events[i].absoluteTick;
    }

    QVector<ScoreSlot> result;
    const QHash<qint64, QVector<int>> noteIndexes = noteIndexesByStartTick(song.notes);
    qint64 absoluteTick = 0;
    while (true)
    {
        CSlot slot = notation.nextSlot();
        if (slot.length() == 0 || isEndSlot(slot))
            break;
        absoluteTick += slot.getDeltaTime();
        result.append(scoreSlot(song, noteIndexes, slot, result.size(), absoluteTick));
    }
    appendBeatMarkers(result, barMap, song.ppqn);
    finishSlotOrder(result);
    return result;
}
