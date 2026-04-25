/*********************************************************************************/
/*!
@file           Notation.cpp

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

#include "Notation.h"
#include "Cfg.h"

#define OPTION_DEBUG_NOTATION     0
#if OPTION_DEBUG_NOTATION
#define ppDEBUG_NOTATION(args)     ppLogDebug  args
#else
#define ppDEBUG_NOTATION(args)
#endif

bool CSlot::addSymbol(CSymbol symbol)
{
    int i;
    if (m_length >= MAX_SYMBOLS)
        return false;

    // Sort the entries low to high
    for (i = m_length - 1; i >= 0; i--)
    {
        if (m_symbols[i].getNote() <= symbol.getNote())
        {
            // don't add duplicates
            if (m_symbols[i].getNote() == symbol.getNote() &&
                m_symbols[i].getType() == symbol.getType() &&
                m_symbols[i].getHand() == symbol.getHand())
                return true;
            break;
        }
        // move the previous entry up one position
        m_symbols[i+1] = m_symbols[i];
    }
    m_symbols[i+1] = symbol;
    m_length++;
    return true;
}

void CSlot::applySplitHands()
{
    if (!CNote::splitHandsEnabled() || m_length == 0)
        return;

    int pitches[MAX_SYMBOLS];
    int noteCount = 0;
    for (int i = 0; i < m_length; ++i)
    {
        if (m_symbols[i].getType() >= PB_SYMBOL_noteHead)
            pitches[noteCount++] = m_symbols[i].getNote();
    }

    if (noteCount == 0)
        return;

    const int splitPoint = CNote::splitPointForPitches(pitches, noteCount);
    for (int i = 0; i < m_length; ++i)
    {
        if (m_symbols[i].getType() >= PB_SYMBOL_noteHead)
            m_symbols[i].setHand(CNote::splitHandForPitch(m_symbols[i].getNote(), splitPoint));
    }
}

// find
void CSlot::analyse()
{
    int i;
    int rightIndex = 0;
    int leftIndex = 0;
    int rightTotal = 0;
    int leftTotal = 0;

    for (i = 0; i < m_length; i++)
    {
        if (m_symbols[i].getType() >= PB_SYMBOL_noteHead)
        {
            if (m_symbols[i].getHand() == PB_PART_right)
                rightTotal++;
            else if (m_symbols[i].getHand() == PB_PART_left)
                leftTotal++;
        }
    }
    for (i = 0; i < m_length; i++)
    {
        if (m_symbols[i].getType() >= PB_SYMBOL_noteHead)
        {
            if (m_symbols[i].getHand() == PB_PART_right)
                m_symbols[i].setIndex(rightIndex++, rightTotal);
            else if (m_symbols[i].getHand() == PB_PART_left)
                m_symbols[i].setIndex(leftIndex++, leftTotal );
        }
    }
}

bool CNotation::m_cfg_displayCourtesyAccidentals = false;

accidentalModifer_t CNotation::detectSuppressedNatural(int note)
{
    if (note <= 0 || note +1 >= MAX_MIDI_NOTES)
        return PB_ACCIDENTAL_MODIFER_noChange;

    accidentalModifer_t modifer = PB_ACCIDENTAL_MODIFER_noChange;
    const qint64 earlyTick = m_absoluteTick + CMidiFile::ppqnAdjust(8);
    const int currentBar = m_barMap == nullptr ? 0 : barAtTick(*m_barMap, earlyTick);

    CNoteState * pNoteState = &m_noteState[note];
    CNoteState * pBackLink = pNoteState->getBackLink();

    int direction = -CStavePos::getStaveAccidentalDirection(note);
    ppDEBUG_NOTATION(("Note %d %d %d", note, direction, pBackLink));
    // check if this note has occurred in this bar before
    if (pNoteState->getBarChange() == currentBar)
    {
        if (pBackLink)
        {
            ppDEBUG_NOTATION(("Force %d", note));
            modifer = PB_ACCIDENTAL_MODIFER_force;
        }
        else if (direction != 0 && m_cfg_displayCourtesyAccidentals == false)
        {
            ppDEBUG_NOTATION(("Suppress %d %d", note, direction));
            modifer = PB_ACCIDENTAL_MODIFER_suppress;
        }
    }

    if (direction != 0)
    {
        // we are display a accidental so force the note above (or below) to display
        m_noteState[note + direction].setBackLink(pNoteState); // point back to this note
        m_noteState[note + direction].setBarChange(currentBar);
        ppDEBUG_NOTATION(("setting backlink %d %d", note + direction, direction));
    }
    if (pBackLink)
    {
        pNoteState->setBackLink(nullptr);
        pBackLink->setBarChange(-1); // this prevents further suppression on the original note
    }

    pNoteState->setBarChange(currentBar);
    return modifer;
}

int CNotation::cfg_param[NOTATE_MAX_PARAMS];

void CNotation::setupNotationParamaters()
{
    cfg_param[NOTATE_semiquaverBoundary] = CMidiFile::ppqnAdjust(DEFAULT_PPQN/4 + 10);
    cfg_param[NOTATE_quaverBoundary]     = CMidiFile::ppqnAdjust(DEFAULT_PPQN/2 + 10);
    cfg_param[NOTATE_crotchetBoundary]   = CMidiFile::ppqnAdjust(DEFAULT_PPQN + 10);
    cfg_param[NOTATE_minimBoundary]      = CMidiFile::ppqnAdjust(DEFAULT_PPQN*2 + 10);
    cfg_param[NOTATE_semibreveBoundary]  = CMidiFile::ppqnAdjust(DEFAULT_PPQN*4 + 10);
}

void CNotation::calculateScoreNoteLength()
{
    if (!Cfg::experimentalNoteLength)
        return;

    CSlot* slot = &m_slots[m_slotReadIndex];
    for (int i = 0; i < slot->length(); i++)
    {
        CSymbol* symbol = slot->getSymbolPtr(i);

        if (symbol-> getType() != PB_SYMBOL_noteHead)
            break;

        // you may get better results assuming all the notes are legato
        // ie assume that this note ends at the exact time the following note starts.
        long midiDuration = symbol->getMidiDuration();

        if (midiDuration < cfg_param[NOTATE_semiquaverBoundary] )
            symbol->setNoteLength(PB_SYMBOL_semiquaver);
        if (midiDuration < cfg_param[NOTATE_quaverBoundary] )
            symbol->setNoteLength(PB_SYMBOL_quaver);
        else if (midiDuration < cfg_param[NOTATE_crotchetBoundary] )
            symbol->setNoteLength(PB_SYMBOL_crotchet);
        else if (midiDuration < cfg_param[NOTATE_minimBoundary] )
            symbol->setNoteLength(PB_SYMBOL_minim);
        else
            symbol->setNoteLength(PB_SYMBOL_semibreve);
    }
}

void CNotation::findNoteSlots()
{
    CMidiEvent midi;
    CSlot slot;

    while (true)
    {
        // Check that some body has put in some events for us
        if (m_midiEventIndex >= m_midiEvents.size())
            break;

        midi = m_midiEvents[m_midiEventIndex++];

        m_currentDeltaTime += midi.deltaTime();
        m_absoluteTick += midi.deltaTime();
        if (midi.type() == MIDI_PB_chordSeparator || midi.type() == MIDI_PB_EOF)
        {
            if (m_currentSlot.length() > 0)
            {
                // the cord separator arrives very late so we are behind the times
                if (CNote::splitHandsNaive() && CNote::splitHandsForChannel(m_displayChannel))
                    m_currentSlot.applySplitHands();
                m_currentSlot.analyse();
                m_slots.append(m_currentSlot);
                m_currentSlot.clear();
            }
            if (midi.type() == MIDI_PB_EOF)
            {
                slot.setSymbol(0, CSymbol( PB_SYMBOL_theEndMarker, PB_PART_both, 0 ));
                m_slots.append(slot);
            }
            break;
        }

        else if (midi.type() == MIDI_PB_keySignature)
            CStavePos::setKeySignature(midi.data1(), midi.data2());
        else if (midi.type() == MIDI_NOTE_ON)
        {
            whichPart_t hand = CNote::findHand( midi, m_displayChannel, PB_PART_both );
            if (hand != PB_PART_none)
            {
                musicalSymbol_t symbolType;
                if (midi.channel() == MIDI_DRUM_CHANNEL)
                    symbolType = PB_SYMBOL_drum;
                else
                    symbolType = PB_SYMBOL_noteHead;
                CSymbol symbol(symbolType, hand, midi.note());
                symbol.setColor(Cfg::noteColor());
                symbol.setMidiDuration(midi.getDuration());

                // check if this note has occurred in this bar before
                symbol.setAccidentalModifer(detectSuppressedNatural(midi.note()));

                if (m_currentSlot.addSymbol(symbol) == false) {
                    ppLogWarn("[%d] Over the Max symbols limit", m_displayChannel + 1);
                }
                m_currentSlot.addDeltaTime(m_currentDeltaTime);
                m_currentDeltaTime = 0;
                if (hand == PB_PART_left)
                {
                    if (midi.note() < MIDI_BOTTOM_C)
                        m_currentSlot.setAv8Left(MIDI_OCTAVE);
                }
            }
        }
    }
}

CSlot CNotation::nextNoteSlot()
{
    // only if the slot queue is empty should we try to find some more
    if (m_slotReadIndex >= m_slots.size())
        findNoteSlots();

    if (m_slotReadIndex < m_slots.size())
    {
        calculateScoreNoteLength();
        return m_slots[m_slotReadIndex++];
    }
    else
        return CSlot(); // this is an empty slot which means end of file
}

CSlot CNotation::nextSlot()
{
    return nextNoteSlot();
}

void CNotation::appendMidiEvent(CMidiEvent event)
{
    if (m_findScrollerChord.findChord(event, m_displayChannel, PB_PART_both ) == true)
    {
        // the Score works differently we just send down a chord separator
        CMidiEvent separator;
        separator.chordSeparator(event);
        m_midiEvents.append(separator);
    }

    m_midiEvents.append(event);
}

void CNotation::reset()
{
    m_currentDeltaTime = 0;
    m_midiEvents.clear();
    m_slots.clear();
    m_midiEventIndex = 0;
    m_slotReadIndex = 0;
    m_currentSlot.clear();
    m_absoluteTick = 0;

    m_findScrollerChord.reset();
    for (auto &noteState : m_noteState)
        noteState.clear();
    setupNotationParamaters();
}
