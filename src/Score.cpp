/*********************************************************************************/
/*!
@file           Score.cpp

@brief          xxxx.

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
#include "Draw.h"
#include "Score.h"
#include "KeyboardGeometry.h"

namespace {
constexpr int SynthesiaVisibleBeats = 8;

float synthesiaLeftX()
{
    return Cfg::staveStartX();
}

float synthesiaWhiteKeyWidth()
{
    const float width = Cfg::staveEndX() - synthesiaLeftX();
    return width / static_cast<float>(KeyboardGeometry::WhiteKeyCount);
}

float synthesiaKeyboardBottomY()
{
    return 22.0f;
}

float synthesiaKeyboardHeight()
{
    return qBound(56.0f, static_cast<float>(Cfg::getAppHeight()) * 0.12f, 88.0f);
}

float synthesiaKeyboardTopY()
{
    return synthesiaKeyboardBottomY() + synthesiaKeyboardHeight();
}

float synthesiaStrikeY()
{
    return synthesiaKeyboardTopY() + 8.0f;
}

float synthesiaTopY()
{
    const float desiredTop = static_cast<float>(Cfg::getAppHeight()) - 92.0f;
    const float minTop = synthesiaStrikeY() + 140.0f;
    return qMin(static_cast<float>(Cfg::getAppHeight()) - 12.0f, qMax(minTop, desiredTop));
}

int synthesiaVisibleTicks()
{
    return qMax(1, CMidiFile::getPulsesPerQuarterNote() * SynthesiaVisibleBeats);
}

float synthesiaPixelsPerTick(float strikeY, float topY)
{
    return (topY - strikeY) / static_cast<float>(synthesiaVisibleTicks());
}

void drawSynthesiaGridLine(float x, float bottomY, float topY)
{
    glBegin(GL_LINES);
    glVertex2f(x, bottomY);
    glVertex2f(x, topY);
    glEnd();
}

void drawHorizontalGuide(float leftX, float rightX, float y)
{
    glBegin(GL_LINES);
    glVertex2f(leftX, y);
    glVertex2f(rightX, y);
    glEnd();
}

void drawSynthesiaGrid(float leftX, float width, float bottomY, float topY)
{
    CDraw::drColorAlpha(Cfg::synthesiaGridColor(), 0.16f);
    glLineWidth(1.0f);
    for (int i = 0; i <= KeyboardGeometry::WhiteKeyCount; ++i)
        drawSynthesiaGridLine(leftX + static_cast<float>(i) * width, bottomY, topY);
}

void drawSynthesiaStrikeLine(float leftX, float rightX, float strikeY)
{
    CDraw::drColorAlpha(Cfg::playZoneCenterColor(), 0.20f);
    glRectf(leftX, strikeY + 7.0f, rightX, strikeY - 7.0f);
    CDraw::drColorAlpha(Cfg::playZoneCenterColor(), 0.95f);
    glLineWidth(2.5f);
    drawHorizontalGuide(leftX, rightX, strikeY);
}

bool beatIsBar(qint64 tick)
{
    const qint64 ppqn = qMax(1, CMidiFile::getPulsesPerQuarterNote());
    return ((tick / ppqn) % 4) == 0;
}

void drawBeatGuide(float leftX, float rightX, float y, bool barLine)
{
    CDraw::drColorAlpha(Cfg::synthesiaGridColor(), barLine ? 0.42f : 0.20f);
    glLineWidth(barLine ? 1.8f : 1.0f);
    drawHorizontalGuide(leftX, rightX, y);
}

void drawSynthesiaBeatGuides(float leftX, float rightX, float strikeY,
                             float topY, qint64 currentTicks)
{
    const qint64 ppqn = qMax(1, CMidiFile::getPulsesPerQuarterNote());
    const qint64 firstOffset = ppqn - ((currentTicks % ppqn + ppqn) % ppqn);
    const float pxPerTick = synthesiaPixelsPerTick(strikeY, topY);
    for (qint64 offset = firstOffset; offset < synthesiaVisibleTicks(); offset += ppqn) {
        const float y = strikeY + static_cast<float>(offset) * pxPerTick;
        drawBeatGuide(leftX, rightX, y, beatIsBar(currentTicks + offset));
    }
}

void drawKeyRect(float left, float bottom, float width, float height, CColor color)
{
    CDraw::drColorAlpha(Cfg::paperShadowColor(), 0.18f);
    glRectf(left + 1.0f, bottom + height - 1.0f, left + width + 1.0f, bottom - 1.0f);
    CDraw::drColor(color);
    glRectf(left, bottom + height, left + width, bottom);
}

void drawKeyOutline(float left, float bottom, float width, float height, float alpha)
{
    CDraw::drColorAlpha(Cfg::pianoKeyEdgeColor(), alpha);
    glBegin(GL_LINE_LOOP);
    glVertex2f(left, bottom + height);
    glVertex2f(left + width, bottom + height);
    glVertex2f(left + width, bottom);
    glVertex2f(left, bottom);
    glEnd();
}

void drawWhiteKey(int note, float leftX, float bottomY, float width, float height)
{
    const float left = KeyboardGeometry::keyLeft(note, leftX, width);
    drawKeyRect(left, bottomY, width * 0.96f, height, Cfg::pianoWhiteKeyColor());
    drawKeyOutline(left, bottomY, width * 0.96f, height, 0.55f);
}

void drawBlackKey(int note, float leftX, float bottomY, float width, float height)
{
    const float keyHeight = height * 0.62f;
    const float left = KeyboardGeometry::keyLeft(note, leftX, width);
    const float keyWidth = KeyboardGeometry::keyWidth(note, width);
    drawKeyRect(left, bottomY + height - keyHeight, keyWidth, keyHeight, Cfg::pianoBlackKeyColor());
    drawKeyOutline(left, bottomY + height - keyHeight, keyWidth, keyHeight, 0.70f);
}

void drawKeyboardLayer(bool blackKeys, float leftX, float bottomY, float width, float height)
{
    for (int note = KeyboardGeometry::LowestMidiNote; note <= KeyboardGeometry::HighestMidiNote; ++note)
    {
        if (KeyboardGeometry::isBlackKey(note) == blackKeys)
        {
            if (blackKeys)
                drawBlackKey(note, leftX, bottomY, width, height);
            else
                drawWhiteKey(note, leftX, bottomY, width, height);
        }
    }
}

void drawChordKeyHighlights(CChord chord, CColor color, float leftX, float bottomY, float width, float height)
{
    for (int i = 0; i < chord.length(); ++i) {
        const int note = chord.getNote(i).pitch();
        const float left = KeyboardGeometry::keyLeft(note, leftX, width);
        const float keyWidth = KeyboardGeometry::keyWidth(note, width);
        const float keyHeight = KeyboardGeometry::isBlackKey(note) ? height * 0.62f : height;
        const float keyBottom = KeyboardGeometry::isBlackKey(note) ? bottomY + height - keyHeight : bottomY;
        CDraw::drColorAlpha(color, 0.24f);
        glRectf(left - 2.0f, keyBottom + keyHeight + 4.0f, left + keyWidth + 2.0f, keyBottom - 2.0f);
        CDraw::drColorAlpha(color, 0.62f);
        glRectf(left, keyBottom + keyHeight, left + keyWidth, keyBottom);
    }
}

void clearKeyLights(CSynthesiaKeyLight *lights, int count)
{
    for (int i = 0; i < count; ++i) {
        lights[i].pitch = KeyboardGeometry::LowestMidiNote + i;
        lights[i].color = Cfg::noteColor();
        lights[i].intensity = 0.0f;
        lights[i].active = false;
    }
}

bool lightIsOnLayer(const CSynthesiaKeyLight& light, bool blackKeys)
{
    return light.active && KeyboardGeometry::isBlackKey(light.pitch) == blackKeys;
}

void drawKeyLightRect(float left, float bottom, float width, float height,
                      CColor color, float intensity)
{
    CDraw::drColorAlpha(color, 0.24f * intensity);
    glRectf(left - 3.0f, bottom + height + 5.0f, left + width + 3.0f, bottom - 3.0f);
    CDraw::drColorAlpha(color, 0.72f * intensity);
    glRectf(left, bottom + height, left + width, bottom);
    CDraw::drColorAlpha(CColor(1.0, 1.0, 1.0), 0.38f * intensity);
    glRectf(left + 1.0f, bottom + height, left + width - 1.0f, bottom + height - 7.0f);
}

void drawSynthesiaKeyLight(const CSynthesiaKeyLight& light, float leftX,
                           float bottomY, float width, float height)
{
    const float left = KeyboardGeometry::keyLeft(light.pitch, leftX, width);
    const float keyWidth = KeyboardGeometry::keyWidth(light.pitch, width);
    const bool blackKey = KeyboardGeometry::isBlackKey(light.pitch);
    const float keyHeight = blackKey ? height * 0.62f : height;
    const float keyBottom = blackKey ? bottomY + height - keyHeight : bottomY;
    drawKeyLightRect(left, keyBottom, keyWidth, keyHeight, light.color, light.intensity);
}

void drawSynthesiaKeyLightLayer(CSynthesiaKeyLight *lights, int count, bool blackKeys,
                                float leftX, float bottomY, float width, float height)
{
    for (int i = 0; i < count; ++i)
        if (lightIsOnLayer(lights[i], blackKeys))
            drawSynthesiaKeyLight(lights[i], leftX, bottomY, width, height);
}
}

CScore::CScore(CSettings* settings) : CDraw(settings)
{
    m_piano = new CPiano(settings);
    m_rating = nullptr;
    for (int i=0; i< arraySize(m_scroll); i++)
    {
        m_scroll[i] = new CScroll(i, settings);
        m_scroll[i]->setChannel(i);
    }

    m_activeScroll = -1;
    m_stavesDisplayListId = 0;
    m_scoreDisplayListId = 0;//glGenLists (1);
}

CScore::~CScore()
{
    delete m_piano;
    for (auto *const scroll : m_scroll)
        delete scroll;

    if (m_scoreDisplayListId != 0)
        glDeleteLists(m_scoreDisplayListId, 1);
    m_scoreDisplayListId = 0;

    if (m_stavesDisplayListId != 0)
        glDeleteLists(m_stavesDisplayListId, 1);
    m_stavesDisplayListId = 0;
}

void CScore::init()
{
}

void CScore::drawScroll(bool refresh)
{
    if (Cfg::viewMode() == PB_VIEW_MODE_synthesia)
    {
        drawSynthesia(refresh);
        return;
    }

    if (refresh == false)
    {
        float topY = CStavePos(PB_PART_right, MAX_STAVE_INDEX).getPosY();
        float bottomY = CStavePos(PB_PART_left, MIN_STAVE_INDEX).getPosY();
        drColor (Cfg::backgroundColor());
        glRectf(Cfg::scrollStartX(), topY, static_cast<float>(Cfg::getAppWidth()), bottomY);
    }

    if (m_stavesDisplayListId == 0)
        m_stavesDisplayListId = glGenLists (1);

    if (getCompileRedrawCount())
    {

        glNewList (m_stavesDisplayListId, GL_COMPILE_AND_EXECUTE);
            drawSymbol(CSymbol(PB_SYMBOL_playingZone,  CStavePos(PB_PART_both, 0)), Cfg::playZoneX());
            drawStaves(Cfg::scrollStartX(), Cfg::staveEndX());
        glEndList ();
                // decrement the compile count until is reaches zero
        forceCompileRedraw(0);

    }
    else
        glCallList(m_stavesDisplayListId);

    if (m_settings->value("View/PianoKeyboard").toString()=="on"){
        drawPianoKeyboard();
    }
    drawScrollingSymbols(true);
    m_piano->drawPianoInput();
}

void CScore::drawSynthesia(bool refresh)
{
    const float leftX = synthesiaLeftX();
    const float width = synthesiaWhiteKeyWidth();
    const float strikeY = synthesiaStrikeY();
    const float topY = synthesiaTopY();
    const float rightX = KeyboardGeometry::keyboardRight(leftX, width);

    if (refresh == false)
    {
        drColor(Cfg::backgroundColor());
        glRectf(0.0f, topY, static_cast<float>(Cfg::getAppWidth()), 10.0f);
    }
    drawSynthesiaGrid(leftX, width, strikeY, topY);
    if (m_settings->synthesiaBeatGuides())
        drawSynthesiaBeatGuides(leftX, rightX, strikeY, topY, currentSynthesiaTicks());
    drawSynthesiaStrikeLine(leftX, rightX, strikeY);
    drawSynthesiaNotes();
    drawSynthesiaKeyboard();
}

qint64 CScore::currentSynthesiaTicks() const
{
    if (m_activeScroll < 0)
        return 0;
    return m_scroll[m_activeScroll]->currentSynthesiaTicks();
}

void CScore::collectSynthesiaKeyLights(CSynthesiaKeyLight *lights, int lightCount)
{
    const float leftX = synthesiaLeftX();
    const float width = synthesiaWhiteKeyWidth();
    const float strikeY = synthesiaStrikeY();
    const float topY = synthesiaTopY();

    for (auto *const scroll : m_scroll)
        scroll->collectSynthesiaKeyLights(strikeY, topY, leftX, width, lights, lightCount);
}

void CScore::drawSynthesiaNotes()
{
    const float leftX = synthesiaLeftX();
    const float width = synthesiaWhiteKeyWidth();
    const float strikeY = synthesiaStrikeY();
    const float topY = synthesiaTopY();

    for (auto *const scroll : m_scroll)
        scroll->drawSynthesiaNotes(strikeY, topY, leftX, width);
}

void CScore::drawSynthesiaKeyboard()
{
    const int keyCount = KeyboardGeometry::HighestMidiNote - KeyboardGeometry::LowestMidiNote + 1;
    CSynthesiaKeyLight lights[keyCount];
    const float leftX = synthesiaLeftX();
    const float width = synthesiaWhiteKeyWidth();
    const float bottomY = synthesiaKeyboardBottomY();
    const float height = synthesiaKeyboardHeight();

    clearKeyLights(lights, keyCount);
    collectSynthesiaKeyLights(lights, keyCount);

    drawKeyboardLayer(false, leftX, bottomY, width, height);
    drawChordKeyHighlights(m_piano->getGoodChord(), Cfg::pianoGoodColor(), leftX, bottomY, width, height);
    drawChordKeyHighlights(m_piano->getBadChord(), Cfg::pianoBadColor(), leftX, bottomY, width, height);
    drawSynthesiaKeyLightLayer(lights, keyCount, false, leftX, bottomY, width, height);
    drawKeyboardLayer(true, leftX, bottomY, width, height);
    drawSynthesiaKeyLightLayer(lights, keyCount, true, leftX, bottomY, width, height);
}

void CScore::drawPianoKeyboard(){
    const static int keysCount = 88;
    struct PianoKeyboard {
        int i, k;
        float yStart;
        float xSize;
        float ySize;

        float xPlaceSize;
        float xKeySize;
        char state[keysCount];
        bool stopped;

        PianoKeyboard() {
            i = 0; k = 0;
            yStart = 0.0f;
            xSize = Cfg::staveEndX() - Cfg::staveStartX();
            ySize = 30;

            xPlaceSize = xSize / 52.0f;
            xKeySize = xPlaceSize - xPlaceSize * 0.1f;
            stopped = false;
        }

        void drawBlackKey(int i, int k) {
            glPushMatrix();
            float yBlackShift = ySize / 2.5f;
            float yBlackSize = ySize - yBlackShift;
            glScalef(1.0f, 1.4f, 1.0f);
            glTranslatef(Cfg::staveStartX() + xPlaceSize * static_cast<float>(i) - xPlaceSize / 3.0f,
                yStart + yBlackShift, 0.0f);

            float xKeySize = this->xKeySize / 1.5f;

            CDraw::drColor (Cfg::pianoBlackKeyColor());
            if(state[k]==1) CDraw::drColor(stopped ? Cfg::playedStoppedColor() : Cfg::noteColor());
            if(state[k]==2) CDraw::drColor(Cfg::playedBadColor());
            glBegin(GL_QUADS);
            glVertex2f(0, yBlackSize);
            glVertex2f(xKeySize, yBlackSize);
            glVertex2f(xKeySize, 0);
            glVertex2f(0, 0);
            glEnd();
            CDraw::drColorAlpha(Cfg::pianoKeyEdgeColor(), 0.65f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(0, yBlackSize);
            glVertex2f(xKeySize, yBlackSize);
            glVertex2f(xKeySize, 0);
            glVertex2f(0, 0);
            glEnd();

            glPopMatrix();
            state[k] = 0;
        }

        void drawWhiteKey() {
            glPushMatrix();
            glScalef(1.0f, 1.4f, 1.0f);
            glTranslatef(Cfg::staveStartX() + xPlaceSize * static_cast<float>(i++), yStart, 0.0f);

            CDraw::drColor (Cfg::pianoWhiteKeyColor());
            if(state[k]==1) CDraw::drColor(stopped ? Cfg::playedStoppedColor() : Cfg::noteColor());
            if(state[k]==2) CDraw::drColor(Cfg::playedBadColor());
            glBegin(GL_QUADS);
            glVertex2f(0, ySize);
            glVertex2f(xKeySize, ySize);
            glVertex2f(xKeySize, 0);
            glVertex2f(0, 0);
            glEnd();
            CDraw::drColorAlpha(Cfg::pianoKeyEdgeColor(), 0.55f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(0, ySize);
            glVertex2f(xKeySize, ySize);
            glVertex2f(xKeySize, 0);
            glVertex2f(0, 0);
            glEnd();

            glPopMatrix();
            state[k++] = 0;
        }

        void drawOctave() {
            drawWhiteKey();
            int b1 = i, k1 = k++;
            drawWhiteKey();
            int b2 = i, k2 = k++;
            drawWhiteKey();
            drawWhiteKey();
            int b3 = i, k3 = k++;
            drawWhiteKey();
            int b4 = i, k4 = k++;
            drawWhiteKey();
            int b5 = i, k5 = k++;
            drawWhiteKey();
            drawBlackKey(b1, k1);
            drawBlackKey(b2, k2);
            drawBlackKey(b3, k3);
            drawBlackKey(b4, k4);
            drawBlackKey(b5, k5);
        }

        void drawKeyboard() {
            i = k = 0;
            drawWhiteKey();
            int b1 = i, k1 = k++;
            drawWhiteKey();
            drawBlackKey(b1, k1);
            for(int i=0; i<7; ++i) drawOctave();
            drawWhiteKey();
        }
    };
    static PianoKeyboard pianoKeyboard;

    CChord chord = m_piano->getBadChord();
    for(int n=0; n<chord.length(); ++n) {
        int pitch = chord.getNote(n).pitch();
        int k = pitch - 21;
        k = k < 0 ? 0 : (k >= keysCount ? (keysCount-1) : k);
        pianoKeyboard.state[k] = 2;
    }

    for (auto *const scroll : m_scroll) {
        int notes[64];
        memset(notes, 0, sizeof(notes));
        bool stopped = scroll->getKeyboardInfo(notes);
        for(int *note=notes; *note; ++note) {
            pianoKeyboard.stopped = stopped;
            int k = *note - 21;
            k = k < 0 ? 0 : (k >= keysCount ? (keysCount-1) : k);
            pianoKeyboard.state[k] = 1;
        }
    }

    pianoKeyboard.drawKeyboard();
}

void CScore::drawScore()
{
    if (Cfg::viewMode() == PB_VIEW_MODE_synthesia)
        return;

    if (getCompileRedrawCount())
    {
        if (m_scoreDisplayListId == 0)
            m_scoreDisplayListId = glGenLists (1);

        glNewList (m_scoreDisplayListId, GL_COMPILE_AND_EXECUTE);
            drColor (Cfg::staveColor());

            drawSymbol(CSymbol(PB_SYMBOL_gClef, CStavePos(PB_PART_right, -1)), Cfg::clefX()); // The Treble Clef
            drawSymbol(CSymbol(PB_SYMBOL_fClef, CStavePos(PB_PART_left, 1)), Cfg::clefX());
            drawKeySignature(CStavePos::getKeySignature());
            drawStaves(Cfg::staveStartX(), Cfg::scrollStartX());
        glEndList ();
    }
    else
        glCallList(m_scoreDisplayListId);
}
