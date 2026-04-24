/*********************************************************************************/
/*!
@file           Scroll.c

@brief          xxx.

@author         L. J. Barman

    Copyright (c)   2008-2013, L. J. Barman, all rights reserved

    This file is part of the PianoBooster application

    PianoBooster is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PianoBooster is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PianoBooster.  If not, see <http://www.gnu.org/licenses/>.

*/
/*********************************************************************************/

#include "Cfg.h"
#include "Scroll.h"
#include "KeyboardGeometry.h"
#include "Settings.h"

//#define NOTE_AHEAD_GAP          50
//#define NOTE_BEHIND_GAP          14

#define NOTE_AHEAD_GAP          22 // the notes on the left hand side of the score
#define NOTE_BEHIND_GAP         14

namespace {
constexpr float SynthesiaMinimumNoteHeight = 14.0f;
constexpr int SynthesiaVisibleBeats = 8;

struct SynthesiaNoteRect
{
    float left;
    float right;
    float bottom;
    float top;
    float alpha;
    CColor color;
    int pitch;
    bool valid;
};

float clampFloat(float value, float minValue, float maxValue)
{
    if (value < minValue)
        return minValue;
    if (value > maxValue)
        return maxValue;
    return value;
}

CColor synthesiaColor(whichPart_t hand)
{
    if (hand == PB_PART_left)
        return Cfg::synthesiaLeftColor();
    return Cfg::synthesiaRightColor();
}

int synthesiaVisibleTicks()
{
    return qMax(1, CMidiFile::getPulsesPerQuarterNote() * SynthesiaVisibleBeats);
}

float synthesiaPixelsPerTick(float strikeY, float topY)
{
    return (topY - strikeY) / static_cast<float>(synthesiaVisibleTicks());
}

CColor synthesiaStateColor(CSymbol symbol)
{
    const CColor color = symbol.getColor();
    if (color == Cfg::playedBadColor())
        return Cfg::playedBadColor();
    if (color == Cfg::playedGoodColor())
        return Cfg::playedGoodColor();
    if (color == Cfg::playedStoppedColor())
        return Cfg::playedStoppedColor();
    return synthesiaColor(symbol.getHand());
}

float handAlpha(whichPart_t hand)
{
    const whichPart_t displayHand = CDraw::getDisplayHand();
    if (displayHand != PB_PART_both && displayHand != hand)
        return 0.25f;
    return 1.0f;
}

float distanceAlpha(float startTicks)
{
    if (startTicks <= 0.0f)
        return 0.92f;
    const float visible = static_cast<float>(synthesiaVisibleTicks());
    return 0.30f + 0.58f * (1.0f - qMin(1.0f, startTicks / visible));
}

void drawSynthesiaRect(float left, float bottom, float right, float top,
                       CColor color, float alpha)
{
    CDraw::drColorAlpha(Cfg::paperShadowColor(), alpha * 0.22f);
    glRectf(left + 1.8f, top + 1.8f, right + 1.8f, bottom + 1.8f);
    CDraw::drColorAlpha(color, alpha);
    glRectf(left, top, right, bottom);
    CDraw::drColorAlpha(CColor(1.0, 1.0, 1.0), alpha * 0.16f);
    glRectf(left + 1.0f, top - 4.0f, right - 1.0f, top);
    CDraw::drColorAlpha(color, qMin(1.0f, alpha + 0.16f));
    glLineWidth(1.2f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(left, top);
    glVertex2f(right, top);
    glVertex2f(right, bottom);
    glVertex2f(left, bottom);
    glEnd();
    CDraw::drColorAlpha(CColor(1.0, 1.0, 1.0), alpha * 0.34f);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    glVertex2f(left + 1.0f, bottom);
    glVertex2f(right - 1.0f, bottom);
    glEnd();
}

void drawStrikeGlow(const SynthesiaNoteRect& rect, float strikeY)
{
    if (rect.bottom > strikeY + 3.0f || rect.top < strikeY)
        return;
    CDraw::drColorAlpha(rect.color, rect.alpha * 0.30f);
    glRectf(rect.left - 4.0f, strikeY + 10.0f, rect.right + 4.0f, strikeY - 5.0f);
    CDraw::drColorAlpha(CColor(1.0, 1.0, 1.0), rect.alpha * 0.50f);
    glLineWidth(3.0f);
    glBegin(GL_LINES);
    glVertex2f(rect.left, strikeY);
    glVertex2f(rect.right, strikeY);
    glEnd();
}

SynthesiaNoteRect makeSynthesiaRect(CSymbol symbol, float startTicks,
                                    float strikeY, float topY,
                                    float leftX, float whiteKeyWidth)
{
    SynthesiaNoteRect rect = {};
    rect.valid = false;
    if (symbol.getType() < PB_SYMBOL_noteHead)
        return rect;

    const float scale = synthesiaPixelsPerTick(strikeY, topY);
    const int pitch = symbol.getNote();
    const float duration = static_cast<float>(qMax(1, symbol.getMidiDuration()));
    const float bottom = strikeY + startTicks * scale;
    const float height = qMax(SynthesiaMinimumNoteHeight, duration * scale);
    rect.left = KeyboardGeometry::keyLeft(pitch, leftX, whiteKeyWidth);
    rect.right = rect.left + KeyboardGeometry::keyWidth(pitch, whiteKeyWidth);
    rect.bottom = clampFloat(bottom, strikeY, topY);
    rect.top = clampFloat(bottom + height, strikeY, topY);
    rect.alpha = distanceAlpha(startTicks) * handAlpha(symbol.getHand());
    rect.color = synthesiaStateColor(symbol);
    rect.pitch = pitch;
    rect.valid = rect.top > strikeY && rect.top > rect.bottom;
    return rect;
}

void drawSynthesiaNote(const SynthesiaNoteRect& rect, float strikeY)
{
    if (!rect.valid)
        return;
    drawStrikeGlow(rect, strikeY);
    drawSynthesiaRect(rect.left, rect.bottom, rect.right, rect.top, rect.color, rect.alpha);
}

bool noteTouchesStrikeLine(const SynthesiaNoteRect& rect, float strikeY)
{
    return rect.valid && rect.bottom <= strikeY + 2.0f && rect.top >= strikeY;
}

void setKeyLight(CSynthesiaKeyLight *lights, int count, const SynthesiaNoteRect& rect)
{
    const int index = rect.pitch - KeyboardGeometry::LowestMidiNote;
    if (index < 0 || index >= count)
        return;
    if (lights[index].active && lights[index].intensity >= rect.alpha)
        return;
    lights[index].pitch = rect.pitch;
    lights[index].color = rect.color;
    lights[index].intensity = qMin(1.0f, rect.alpha + 0.08f);
    lights[index].active = true;
}
}

void CScroll::compileSlot(CSlotDisplayList info)
{

    if (m_show == false || info.m_displayListId == 0)
        return;

    glNewList (info.m_displayListId, GL_COMPILE);
    glTranslatef (static_cast<float>(info.getDeltaTime()) * m_noteSpacingFactor, 0.0f, 0.0f); /*  move position  */

    info.transpose(m_transpose);
    drawSlot(&info);
    /*
    int i;
    CStavePos stavePos;
    int av8Left = info.getAv8Left();
    for (i=0; i < info.length(); i++)
    {
        stavePos.notePos(info.getSymbol(i).getHand(), info.getSymbol(i).getNote());
        //ppLogTrace ("compileSlot len %d id %2d next %2d time %2d type %2d note %2d", info.length(), info.m_displayListId,
        //info.m_nextDisplayListId, info.getDeltaTime(), info.getSymbol(i).getType(), info.getSymbol(i).getNote());

        drawSymbol(info.getSymbol(i), 0.0, stavePos.getPosYRelative()); // we add this  back when drawing this symbol
    }
    */
    glCallList (info.m_nextDisplayListId);    /* Automatically draw the next slot even if it is not there yet */
    glEndList ();
}

/*! Insert a symbol into the display list
 * @return  false when we have run out of symbols
 */
bool CScroll::insertSlots()
{
    GLuint nextListId = 0;

    if (m_headSlot.length() == 0)
        m_headSlot = m_notation->nextSlot();
    if (m_headSlot.length() == 0 || m_headSlot.getSymbolType(0) == PB_SYMBOL_theEndMarker) // this means we have reached the end of the file
        return false;

    while (true)
    {
        float headDelta = deltaAdjustF(m_deltaHead) * m_noteSpacingFactor;
        float slotDetlta = Cfg::staveEndX() - Cfg::playZoneX() - static_cast<float>(m_headSlot.getDeltaTime()) * m_noteSpacingFactor - NOTE_BEHIND_GAP;

        if (headDelta > slotDetlta)
            break;

        if (m_show)
        {
            if (m_symbolID == 0)
                m_symbolID = glGenLists (1);

            nextListId = glGenLists (1);
        }
        else
        {
            nextListId = m_symbolID = 0;
        }

        CSlotDisplayList info(m_headSlot, m_symbolID, nextListId);

        m_deltaHead += info.getDeltaTime() * SPEED_ADJUST_FACTOR;

        compileSlot(info);

        m_scrollQueue->push(info);
        m_symbolID  = nextListId;

        m_headSlot = m_notation->nextSlot();
        if (m_headSlot.length() == 0 || m_headSlot.getSymbolType(0) == PB_SYMBOL_theEndMarker) // this means we have reached the end of the file
            return false;
    }
    return true;
}

void CScroll::removeEarlyTimingMakers()
{
    float delta = deltaAdjustF(m_deltaTail) * m_noteSpacingFactor  + Cfg::playZoneX() - Cfg::scrollStartX() - NOTE_AHEAD_GAP;
    // only look a few steps (10) into the scroll queue
    for (int i = 0; i < 10 && i < m_scrollQueue->length(); i++ )
    {
        if (delta < -(static_cast<float>(m_scrollQueue->index(i).getLeftSideDeltaTime()) * m_noteSpacingFactor))
        {
            m_scrollQueue->indexPtr(i)->clearAllNoteTimmings();
            compileSlot(m_scrollQueue->index(i));
        }
        delta += static_cast<float>(m_scrollQueue->index(i).getDeltaTime()) * m_noteSpacingFactor;
    }
}

void CScroll::removeSlots()
{
    while (m_scrollQueue->length() > 0)
    {
        if (deltaAdjustF(m_deltaTail) * m_noteSpacingFactor > -Cfg::playZoneX() + Cfg::scrollStartX() + NOTE_AHEAD_GAP -(static_cast<float>(m_scrollQueue->index(0).getLeftSideDeltaTime()) * m_noteSpacingFactor) )
            break;

        CSlotDisplayList info = m_scrollQueue->pop();

        m_deltaTail += info.getDeltaTime() * SPEED_ADJUST_FACTOR;

        //ppLogTrace("Remove slot id %2d time %2d type %2d note %2d", info.m_displayListId, info.getDeltaTime(), info.getSymbol(0).getType(), info.getSymbol(0).getNote());

        if (info.m_displayListId)
            glDeleteLists( info.m_displayListId, 1);
        if (m_wantedIndex > 0)
            m_wantedIndex--;  // also the Chord has moved down one place
        else
        {
            m_wantedIndex = 0;
            m_wantedDelta = m_deltaTail;
        }
    }
}

//! Draw all the symbols that we have in the list
void CScroll::drawScrollingSymbols(bool show)
{
    insertSlots();  // new symbols at the end of the score
    removeSlots();  // delete old symbols no longer required
    removeEarlyTimingMakers();

    if (show == false)   // Just update the queue only
        return;

    if (m_scrollQueue->length() == 0 || m_scrollQueue->indexPtr(0)->m_displayListId == 0)
        return;

    glPushMatrix();
    glTranslatef (Cfg::playZoneX() + deltaAdjustF(m_deltaTail) * m_noteSpacingFactor, CStavePos::getStaveCenterY(), 0.0f);

    BENCHMARK(8, "glTranslatef");

    if (m_scrollQueue->length() > 0)
        glCallList (m_scrollQueue->indexPtr(0)->m_displayListId);
    BENCHMARK(9, "glCallList");

    glPopMatrix();
}

void CScroll::drawSynthesiaNotes(float strikeY, float topY, float leftX, float whiteKeyWidth)
{
    insertSlots();
    removeSlots();
    removeEarlyTimingMakers();

    if (m_show == false)
        return;

    float startTicks = deltaAdjustF(m_deltaTail);
    for (int i = 0; i < m_scrollQueue->length(); ++i)
    {
        CSlotDisplayList *slot = m_scrollQueue->indexPtr(i);
        startTicks += static_cast<float>(slot->getDeltaTime());
        for (int j = 0; j < slot->length(); ++j) {
            const SynthesiaNoteRect rect = makeSynthesiaRect(
                        slot->getSymbol(j), startTicks, strikeY, topY, leftX, whiteKeyWidth);
            drawSynthesiaNote(rect, strikeY);
            if (rect.valid && m_settings->synthesiaNoteNames() && rect.top - rect.bottom >= 24.0f)
                drawNoteName(rect.pitch, (rect.left + rect.right) / 2.0f, (rect.bottom + rect.top) / 2.0f, 0);
        }
    }
}

void CScroll::collectSynthesiaKeyLights(float strikeY, float topY, float leftX, float whiteKeyWidth,
                                        CSynthesiaKeyLight *lights, int lightCount)
{
    if (m_show == false || lights == nullptr || lightCount <= 0)
        return;

    float startTicks = deltaAdjustF(m_deltaTail);
    for (int i = 0; i < m_scrollQueue->length(); ++i)
    {
        CSlotDisplayList *slot = m_scrollQueue->indexPtr(i);
        startTicks += static_cast<float>(slot->getDeltaTime());
        for (int j = 0; j < slot->length(); ++j) {
            const SynthesiaNoteRect rect = makeSynthesiaRect(
                        slot->getSymbol(j), startTicks, strikeY, topY, leftX, whiteKeyWidth);
            if (noteTouchesStrikeLine(rect, strikeY))
                setKeyLight(lights, lightCount, rect);
        }
    }
}

void CScroll::scrollDeltaTime(qint64 ticks)
{
    m_deltaHead -= ticks;
    m_deltaTail -= ticks;
    m_wantedDelta -= ticks;
}

bool CScroll::validPianistChord(int index)
{
    CSlot* pSlot = m_scrollQueue->indexPtr(index);

    assert(pSlot->length()!=0);
    if (pSlot->getSymbol(0).getType() >= PB_SYMBOL_noteHead)
    {
        if (m_displayHand == PB_PART_both)
            return true;

        //eventually we need two slot queues one for each hand
        for (int i = 0; i < pSlot->length(); i++)
        {

            if (pSlot->getSymbol(i).getHand() ==  m_displayHand)
                return true;
        }
    }
    return false;
}

int CScroll::findWantedChord(int note, CColor color, qint64 wantedDelta)
{
    Q_UNUSED(note)
    if (color == Cfg::playedBadColor()) // fixme should be an enum
        return m_wantedIndex;
    {
        while ( m_wantedIndex + 1 < m_scrollQueue->length())
        {
            if ((m_wantedDelta + m_scrollQueue->indexPtr(m_wantedIndex)->getDeltaTime() * SPEED_ADJUST_FACTOR) >= -wantedDelta)
            {
                if (validPianistChord(m_wantedIndex) == true)
                    break;
            }
            m_wantedDelta += m_scrollQueue->indexPtr(m_wantedIndex)->getDeltaTime() * SPEED_ADJUST_FACTOR;
            m_wantedIndex++;
        }
    }
    return m_wantedIndex;
}

void CScroll::setPlayedNoteColor(int note, CColor color, qint64 wantedDelta, qint64 pianistTimming)
{
    int index;
    if (m_wantedIndex >= m_scrollQueue->length())
        return;
    index = findWantedChord(note, color, wantedDelta);
    note -= m_transpose;
    m_scrollQueue->indexPtr(index)->setNoteColor(note, color);
    if (pianistTimming != NOT_USED)
    {
        pianistTimming = deltaAdjustL(pianistTimming) * DEFAULT_PPQN / CMidiFile::getPulsesPerQuarterNote();

        m_scrollQueue->indexPtr(index)->setNoteTimming(note, pianistTimming);
    }
    compileSlot(m_scrollQueue->index(index));
}

void CScroll::refresh()
{
    int i;
    if (m_show == false)
        return;

    for ( i = 0; i < m_scrollQueue->length(); i++)
        compileSlot(m_scrollQueue->index(i));
}

bool CScroll::getKeyboardInfo(int *notes)
{
    int stoppedScrollIdx = -1;
    for(int i=0; i<m_scrollQueue->length(); ++i) {
        CSlotDisplayList &info = *m_scrollQueue->indexPtr(i);
        if(m_show == false || info.m_displayListId == 0) continue;

        CSlot* slot = &info;
        for(int j=0; j<slot->length(); ++j) {
            if(slot->getSymbol(j).getType() < PB_SYMBOL_noteHead) continue;
            if(slot->getSymbol(j).getColor() == Cfg::playedStoppedColor()) {
                stoppedScrollIdx = i;
                break;
            }
        }
        if(stoppedScrollIdx > -1) break;
    }
    if(stoppedScrollIdx > -1) {
        for(int i=0; i<stoppedScrollIdx; ++i) {
            CSlotDisplayList &info = *m_scrollQueue->indexPtr(i);
            if(m_show == false || info.m_displayListId == 0) continue;

            CSlot* slot = &info;
            for(int j=0; j<slot->length(); ++j) {
                if(slot->getSymbol(j).getType() < PB_SYMBOL_noteHead) continue;
                slot->getSymbolPtr(j)->setColor(Cfg::playedGoodColor());
            }
        }
    }

    int *note = notes;
    for(int i=0; i<m_scrollQueue->length(); ++i) {
        CSlotDisplayList &info = *m_scrollQueue->indexPtr(i);
        if(m_show == false || info.m_displayListId == 0) continue;

        CSlot* slot = &info;
        bool stopped = false;
        for(int j=0; j<slot->length(); ++j) {
            if(slot->getSymbol(j).getType() < PB_SYMBOL_noteHead) continue;

            if(slot->getSymbol(j).getColor() == Cfg::noteColor() ||
               slot->getSymbol(j).getColor() == Cfg::playedStoppedColor())
                *(note++) = slot->getSymbol(j).getNote();
            if(slot->getSymbol(j).getColor() == Cfg::playedStoppedColor()) stopped = true;
        }
        if(note != notes) return stopped;
    }
    return false;
}

void CScroll::transpose(int transpose)
{
    if (m_transpose == transpose)
        return;

    m_transpose = transpose;
    refresh();
}

void CScroll::showScroll(bool show)
{
    int i;
    GLuint nextListId = 0;

    m_show = show;
    if (show == true)
    {

        if (m_symbolID == 0)
            m_symbolID = glGenLists (1);

        // add in the missing GL display list
        for ( i = 0; i < m_scrollQueue->length(); i++)
        {
            //assert (m_scrollQueue->indexPtr(i)->m_displayListId == 0);
            nextListId = glGenLists (1);
            m_scrollQueue->indexPtr(i)->m_displayListId = m_symbolID;
            m_scrollQueue->indexPtr(i)->m_nextDisplayListId = nextListId;
            m_symbolID  = nextListId;

        }
        // And now compile the slot (remember that each slot points to the next one)
        for ( i = 0; i < m_scrollQueue->length(); i++)
        {
            compileSlot(m_scrollQueue->index(i));
        }
    }
    else
    {
        // Remove all the gl items
        for ( i = 0; i < m_scrollQueue->length(); i++)
        {
            if (m_scrollQueue->indexPtr(i)->m_displayListId != 0 && m_scrollQueue->indexPtr(i)->m_displayListId != nextListId)
                glDeleteLists(m_scrollQueue->index(i).m_displayListId, 1);
            m_scrollQueue->indexPtr(i)->m_displayListId = 0;

            nextListId = m_scrollQueue->indexPtr(i)->m_nextDisplayListId;
            m_scrollQueue->indexPtr(i)->m_nextDisplayListId = 0;

            if (nextListId != 0)
                glDeleteLists(nextListId, 1);

        }
        if (m_symbolID != 0)
           glDeleteLists(m_symbolID, 1);
        m_symbolID = 0;
    }
}

CScroll::CSlotDisplayList::CSlotDisplayList(const CSlot& slot, GLuint displayListId, GLuint nextDisplayListId) : CSlot(slot),
    m_displayListId(displayListId), m_nextDisplayListId(nextDisplayListId)
{
    // It is all done in the initialisation list
}

void CScroll::reset()
{
    int i;
    m_wantedIndex = 0;
    m_wantedDelta = 0;
    m_deltaHead = m_deltaTail = 0;
    m_notation->reset();
    m_headSlot.clear();
    for ( i = 0; i < m_scrollQueue->length(); i++)
    {
        if (m_scrollQueue->index(i).m_displayListId)
            glDeleteLists(m_scrollQueue->index(i).m_displayListId, 1);
    }
    if (m_symbolID != 0)
       glDeleteLists(m_symbolID, 1);
    m_symbolID = 0;

    m_scrollQueue->clear();
    m_ppqnFactor = static_cast<float>(DEFAULT_PPQN) / static_cast<float>(CMidiFile::getPulsesPerQuarterNote());
    m_noteSpacingFactor = m_ppqnFactor * HORIZONTAL_SPACING_FACTOR;
}
