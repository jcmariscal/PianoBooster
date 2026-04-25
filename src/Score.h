/*********************************************************************************/
/*!
@file           Score.h

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

#ifndef _SCORE_H_
#define _SCORE_H_

#include "Piano.h"
#include "PracticeFeedback.h"
#include "ScoreSlot.h"
#include "ScoreViewport.h"
#include "Settings.h"
#include "SongData.h"

class CSlot;

struct CSynthesiaKeyLight
{
    int pitch = -1;
    CColor color;
    float intensity = 0.0f;
    bool active = false;
};

class CScore : public CDraw
{
public:

    CScore(CSettings* settings);

    ~CScore();

    void init();

    void setSongData(const SongData& song);
    void seekToTick(qint64 tick);
    void setCurrentTick(qint64 tick);
    void invalidateActiveScoreCache()
    {
        CDraw::forceCompileRedraw();
    }
    void invalidateStaticScoreCache()
    {
        CDraw::forceCompileRedraw();
    }
    void invalidateRendererCaches()
    {
        invalidateStaticScoreCache();
        invalidateActiveScoreCache();
    }

    void transpose(int semitones);

    void reset();

    void drawScrollingSymbols(const ScoreViewport& viewport, bool show = true);

    void setRatingObject(CRating* rating)
    {
        m_rating = rating;
    }

    CPiano* getPianoObject() { return m_piano;}

    void setNoteFeedback(int note, PracticeFeedbackKind kind,
                         qint64 wantedDelta, qint64 pianistTimming = NOT_USED);

    void setActiveChannel(int channel)
    {
        if (channel < 0 || channel >= MAX_MIDI_CHANNELS)
            return;

        if (m_activeScroll != channel)
            m_activeScroll = channel;
    }

    void setDisplayHand(whichPart_t hand)
    {
        CDraw::setDisplayHand(hand);
        invalidateRendererCaches();
    }

    void drawScore();
    void drawScroll(bool refresh);
    void drawPianoKeyboard();

protected:
    CPiano* m_piano;

private:
    struct ScoreFeedback
    {
        bool active = false;
        int pitch = -1;
        int slotId = -1;
        int noteId = -1;
        PracticeFeedbackKind kind = PracticeFeedbackPending;
        qint64 timing = NOT_USED;
    };

    struct ScoreFeedbackTarget
    {
        int slotId = -1;
        int noteId = -1;
    };

    ScoreViewport currentViewport() const;
    qint64 currentSynthesiaTicks() const;
    void collectSynthesiaKeyLights(const ScoreViewport& viewport,
                                   CSynthesiaKeyLight *lights, int lightCount);
    void drawSynthesia(bool refresh, const ScoreViewport& viewport);
    void drawSynthesiaKeyboard(const ScoreViewport& viewport);
    void drawSynthesiaNotes(const ScoreViewport& viewport);
    void drawSynthesiaNoteBodies(const ScoreViewport& viewport);
    void drawSynthesiaNoteLabels(const ScoreViewport& viewport);
    void drawScoreSlot(const ScoreSlot& scoreSlot, const ScoreViewport& viewport);
    void collectScoreKeyLights(const ScoreViewport& viewport,
                               CSynthesiaKeyLight *lights, int lightCount) const;
    void applySlotFeedback(CSlot *slot, const ScoreSlot& scoreSlot) const;
    void clearFeedback();
    void updateFeedback(int note, PracticeFeedbackKind kind,
                        qint64 wantedDelta, qint64 pianistTimming);
    ScoreFeedbackTarget feedbackTarget(int pitch, qint64 targetTick) const;
    const ScoreFeedback* feedbackFor(int noteId, int pitch, int slotId) const;

    CRating* m_rating;
    QVector<ScoreSlot> m_scoreSlots[MAX_MIDI_CHANNELS];
    QVector<NoteEvent> m_noteEvents;
    ScoreFeedback m_feedback[MAX_MIDI_NOTES];
    qint64 m_currentTick;
    int m_activeScroll;
    int m_transpose;
    GLuint m_scoreDisplayListId;
    GLuint m_stavesDisplayListId;

};

#endif // _SCORE_H_
