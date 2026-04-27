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
#include "ChordAnnotationPlayback.h"
#include "Draw.h"
#include "Notation.h"
#include "NoteIndex.h"
#include "Score.h"
#include "KeyboardGeometry.h"
#include "NotationBuilder.h"
#include "ScoreIndex.h"
#include "Util.h"

#include <algorithm>
#include <limits>

namespace {
constexpr int SynthesiaVisibleBeats = 8;
constexpr float SynthesiaMinimumNoteHeight = 14.0f;
constexpr float SynthesiaLabelMargin = 9.0f;
constexpr const char AnnotatedChordPlayModeSetting[] = "Song/AnnotatedChordPlayMode";
constexpr const char ProCompingStylesSetting[] = "Song/AnnotatedChordProCompingStyles";
constexpr const char SynthesiaCompingViewSetting[] = "View/SynthesiaComping";
constexpr int SynthesiaCompingChannel = MAX_MIDI_CHANNELS - 1;

struct SynthesiaNoteRect
{
    float left = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float top = 0.0f;
    float alpha = 0.0f;
    CColor color;
    int pitch = -1;
    bool valid = false;
};

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

AnnotatedChordPlayMode scoreAnnotatedChordPlayMode(const CSettings *settings)
{
    const int mode = settings == nullptr ? AnnotatedChordPlayRootChord :
                settings->value(AnnotatedChordPlayModeSetting,
                                AnnotatedChordPlayRootChord).toInt();
    if (mode == AnnotatedChordPlayComping || mode == AnnotatedChordPlayProComping)
        return static_cast<AnnotatedChordPlayMode>(mode);
    return AnnotatedChordPlayRootChord;
}

int scoreProCompingStyleMask(const CSettings *settings)
{
    const int mask = settings == nullptr ? AnnotatedChordProStyleSimple :
                settings->value(ProCompingStylesSetting,
                                AnnotatedChordProStyleSimple).toInt() &
                AnnotatedChordProStyleAll;
    return mask == 0 ? AnnotatedChordProStyleSimple : mask;
}

int synthesiaVisualEventPriority(const MidiEventRecord& record)
{
    if (record.event.type() == MIDI_NOTE_OFF)
        return 0;
    if (record.event.type() == MIDI_NOTE_ON && record.event.velocity() <= 0)
        return 0;
    return 1;
}

bool closesVisualNote(const MidiEventRecord& record)
{
    return record.event.type() == MIDI_NOTE_OFF ||
            (record.event.type() == MIDI_NOTE_ON && record.event.velocity() <= 0);
}

NoteEvent synthesiaCompingNote(const MidiEventRecord& record, int id)
{
    NoteEvent note;
    note.id = id;
    note.startTick = record.absoluteTick;
    note.endTick = record.absoluteTick + 1;
    note.pitch = record.event.note();
    note.velocity = record.event.velocity();
    note.channel = record.event.channel();
    note.track = record.track;
    note.hand = CNote::splitHandForPitch(note.pitch, MIDDLE_C);
    return note;
}

QVector<NoteEvent> noteEventsFromPlaybackEvents(QVector<MidiEventRecord> events,
                                                qint64 songEndTick)
{
    std::stable_sort(events.begin(), events.end(), [](const MidiEventRecord& left,
                     const MidiEventRecord& right) {
        if (left.absoluteTick != right.absoluteTick)
            return left.absoluteTick < right.absoluteTick;
        return synthesiaVisualEventPriority(left) < synthesiaVisualEventPriority(right);
    });
    QVector<NoteEvent> notes;
    int active[MAX_MIDI_NOTES];
    std::fill(active, active + MAX_MIDI_NOTES, -1);
    qint64 endTick = qMax<qint64>(1, songEndTick);
    for (const MidiEventRecord& record : events)
    {
        const int pitch = record.event.note();
        if (pitch < 0 || pitch >= MAX_MIDI_NOTES)
            continue;
        endTick = qMax(endTick, record.absoluteTick);
        if (record.event.type() == MIDI_NOTE_ON && record.event.velocity() > 0)
        {
            if (active[pitch] >= 0)
                notes[active[pitch]].endTick =
                        qMax(record.absoluteTick, notes[active[pitch]].startTick + 1);
            const NoteEvent note = synthesiaCompingNote(record, notes.size());
            active[pitch] = notes.size();
            notes.append(note);
        }
        else if (closesVisualNote(record) && active[pitch] >= 0)
        {
            NoteEvent& note = notes[active[pitch]];
            note.endTick = qMax(record.absoluteTick, note.startTick + 1);
            active[pitch] = -1;
        }
    }
    for (int pitch = 0; pitch < MAX_MIDI_NOTES; pitch++)
        if (active[pitch] >= 0)
            notes[active[pitch]].endTick = qMax(endTick, notes[active[pitch]].startTick + 1);
    return notes;
}

float clampFloat(float value, float minValue, float maxValue)
{
    if (value < minValue)
        return minValue;
    if (value > maxValue)
        return maxValue;
    return value;
}

whichPart_t noteHand(const NoteEvent& note)
{
    if (note.hand == PB_PART_left || note.hand == PB_PART_right)
        return static_cast<whichPart_t>(note.hand);
    if (note.channel == CNote::rightHandChan())
        return PB_PART_right;
    if (note.channel == CNote::leftHandChan())
        return PB_PART_left;
    return CNote::splitHandForPitch(note.pitch, MIDDLE_C);
}

bool noteVisibleForChannel(const NoteEvent& note, int displayChannel)
{
    if (displayChannel < 0)
        return false;
    if (note.channel == displayChannel)
        return true;
    if (note.channel >= 0 && note.channel < MAX_MIDI_CHANNELS &&
            CNote::splitHandsCreateChannels() &&
            CNote::splitHandsForChannel(displayChannel) &&
            CNote::rightHandTrack(displayChannel) >= 0 &&
            CNote::rightHandTrack(note.channel) >= 0)
        return true;
    if (CNote::splitHandsForChannel(displayChannel) && CNote::splitHandsForChannel(note.channel))
        return true;
    return CNote::hasPianoPart(displayChannel) && CNote::hasPianoPart(note.channel);
}

float handAlpha(whichPart_t selectedHand, whichPart_t hand)
{
    if (selectedHand != PB_PART_both && selectedHand != hand)
        return 0.25f;
    return 1.0f;
}

float distanceAlpha(float startTicks, qint64 visibleTicks)
{
    if (startTicks <= 0.0f)
        return 0.92f;
    const float visible = static_cast<float>(qMax<qint64>(1, visibleTicks));
    return 0.30f + 0.58f * (1.0f - qMin(1.0f, startTicks / visible));
}

CColor synthesiaColor(whichPart_t hand)
{
    if (hand == PB_PART_left)
        return Cfg::synthesiaLeftColor();
    return Cfg::synthesiaRightColor();
}

CColor practiceFeedbackColor(PracticeFeedbackKind kind)
{
    if (kind == PracticeFeedbackGood)
        return Cfg::playedGoodColor();
    if (kind == PracticeFeedbackBad)
        return Cfg::playedBadColor();
    if (kind == PracticeFeedbackStopped)
        return Cfg::playedStoppedColor();
    if (kind == PracticeFeedbackMissed)
        return Cfg::pianoBadColor();
    return Cfg::noteColor();
}

float synthesiaPixelsPerTick(float strikeY, float topY)
{
    return (topY - strikeY) / static_cast<float>(synthesiaVisibleTicks());
}

float scorePixelsPerTick()
{
    const int ppqn = qMax(1, CMidiFile::getPulsesPerQuarterNote());
    return static_cast<float>(DEFAULT_PPQN) / static_cast<float>(ppqn) *
            HORIZONTAL_SPACING_FACTOR;
}

qint64 scoreVisibleTicks(float pixelsPerTick)
{
    return viewportVisibleTicksForPixels(Cfg::staveEndX() - Cfg::scrollStartX(),
                                         pixelsPerTick);
}

qint64 scoreLeftTicks(float pixelsPerTick)
{
    return viewportVisibleTicksForPixels(Cfg::playZoneX() - Cfg::scrollStartX(),
                                         pixelsPerTick);
}

qint64 tickDistance(qint64 a, qint64 b)
{
    return a > b ? a - b : b - a;
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

bool noteTouchesStrikeLine(const SynthesiaNoteRect& rect, float strikeY)
{
    return rect.valid && rect.bottom <= strikeY + 2.0f && rect.top >= strikeY;
}

float synthesiaLabelY(const SynthesiaNoteRect& rect, float strikeY, float topY)
{
    const float centerY = (rect.bottom + rect.top) / 2.0f;
    return clampFloat(centerY, strikeY + SynthesiaLabelMargin, topY - SynthesiaLabelMargin);
}

SynthesiaNoteRect makeSynthesiaRect(const NoteEvent& note, const ScoreViewport& viewport,
                                    float strikeY, float topY, float leftX,
                                    float whiteKeyWidth, CColor color)
{
    SynthesiaNoteRect rect;
    const float startTicks = static_cast<float>(note.startTick - viewport.originTick);
    const float durationTicks = static_cast<float>(qMax<qint64>(1, note.endTick - note.startTick));
    const float bottom = strikeY + startTicks * viewport.pixelsPerTick;
    const float height = qMax(SynthesiaMinimumNoteHeight, durationTicks * viewport.pixelsPerTick);

    rect.left = KeyboardGeometry::keyLeft(note.pitch, leftX, whiteKeyWidth);
    rect.right = rect.left + KeyboardGeometry::keyWidth(note.pitch, whiteKeyWidth);
    rect.bottom = clampFloat(bottom, strikeY, topY);
    rect.top = clampFloat(bottom + height, strikeY, topY);
    rect.alpha = distanceAlpha(startTicks, viewport.visibleTicks) *
            handAlpha(viewport.selectedHand, noteHand(note));
    rect.color = color;
    rect.pitch = note.pitch;
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

void setKeyLight(CSynthesiaKeyLight *lights, int count, int pitch, CColor color, float intensity)
{
    const int index = pitch - KeyboardGeometry::LowestMidiNote;
    if (index < 0 || index >= count)
        return;
    if (lights[index].active && lights[index].intensity >= intensity)
        return;
    lights[index].pitch = pitch;
    lights[index].color = color;
    lights[index].intensity = qMin(1.0f, intensity);
    lights[index].active = true;
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

    m_activeScroll = -1;
    m_currentTick = 0;
    m_transpose = 0;
    m_stavesDisplayListId = 0;
    m_scoreDisplayListId = 0;//glGenLists (1);
    clearFeedback();
}

CScore::~CScore()
{
    delete m_piano;

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

void CScore::reset()
{
    m_currentTick = 0;
    clearFeedback();
}

void CScore::transpose(int semitones)
{
    m_transpose = semitones;
}

void CScore::setSongData(const SongData& song)
{
    m_noteEvents = song.notes;
    m_chordAnnotations = song.chordAnnotations;
    setSynthesiaCompingData(song);
    clearFeedback();
    for (int channel = 0; channel < arraySize(m_scoreSlots); channel++)
    {
        m_scoreSlots[channel] = buildNotationSlots(song, channel);
    }
}

void CScore::setSynthesiaCompingData(const SongData& song)
{
    const QVector<MidiEventRecord> events = buildAnnotatedChordPlaybackEvents(
                song, SynthesiaCompingChannel, AnnotatedChordPlaybackVelocity,
                scoreAnnotatedChordPlayMode(m_settings),
                scoreProCompingStyleMask(m_settings));
    m_synthesiaCompingNotes = noteEventsFromPlaybackEvents(events, song.durationTicks);
}

void CScore::seekToTick(qint64 tick)
{
    setCurrentTick(tick);
    clearFeedback();
}

void CScore::setCurrentTick(qint64 tick)
{
    m_currentTick = tick > 0 ? tick : 0;
}

ScoreViewport CScore::currentViewport() const
{
    const int mode = Cfg::viewMode();
    if (mode == PB_VIEW_MODE_synthesia)
    {
        const float strikeY = synthesiaStrikeY();
        const float topY = synthesiaTopY();
        return makeScoreViewport(currentSynthesiaTicks(), synthesiaVisibleTicks(),
                                 synthesiaPixelsPerTick(strikeY, topY),
                                 CDraw::getDisplayHand(), mode);
    }

    const float pixelsPerTick = scorePixelsPerTick();
    return makeScoreViewport(currentSynthesiaTicks(), scoreVisibleTicks(pixelsPerTick),
                             pixelsPerTick, CDraw::getDisplayHand(), mode);
}

void CScore::drawScoreSlot(const ScoreSlot& scoreSlot, const ScoreViewport& viewport)
{
    CSlot slot;
    slot.setAv8Left(scoreSlot.av8Left);
    for (int i = 0; i < scoreSlot.symbols.size(); i++)
    {
        const ScoreSlotSymbol& source = scoreSlot.symbols[i];
        CSymbol symbol(source.type, source.hand, source.midiNote);
        symbol.setMidiDuration(source.midiDuration);
        symbol.setAccidentalModifer(source.accidentalModifier);
        symbol.setIndex(source.noteIndex, source.noteTotal);
        symbol.setColor(Cfg::noteColor());
        slot.addSymbol(symbol);
    }
    applySlotFeedback(&slot, scoreSlot);
    slot.transpose(m_transpose);

    glPushMatrix();
    glTranslatef(Cfg::playZoneX() + viewportTickOffsetPixels(viewport, scoreSlot.absoluteTick),
                 CStavePos::getStaveCenterY(), 0.0f);
    drawSlot(&slot);
    glPopMatrix();
}

void CScore::drawScrollingSymbols(const ScoreViewport& viewport, bool show)
{
    if (!show || m_activeScroll < 0)
        return;

    const QVector<ScoreSlot>& scoreSlots = m_scoreSlots[m_activeScroll];
    const qint64 leftTicks = scoreLeftTicks(viewport.pixelsPerTick);
    const QVector<int> indexes = visibleScoreSlotIndexes(
                scoreSlots, viewport.originTick - leftTicks, viewport.visibleTicks);

    for (int i = 0; i < indexes.size(); i++)
        drawScoreSlot(scoreSlots[indexes[i]], viewport);
}

void CScore::drawChordAnnotations(const ScoreViewport& viewport)
{
#ifndef NO_USE_FTGL
    if (m_settings == nullptr || !m_settings->value("Song/AnnotateScore", false).toBool())
        return;
    const int keySignature = CStavePos::getKeySignature();
    if (viewport.viewMode == PB_VIEW_MODE_synthesia)
    {
        const float x = synthesiaLeftX() + 42.0f;
        const float strikeY = synthesiaStrikeY();
        const float topY = synthesiaTopY();
        float lastY = strikeY - 1000.0f;
        for (const ChordAnnotation& annotation : m_chordAnnotations)
        {
            const QString label = transposedChordAnnotationLabel(annotation, m_transpose, keySignature);
            if (label.isEmpty())
                continue;
            const float y = strikeY + viewportTickOffsetPixels(viewport, annotation.startTick);
            if (y < strikeY + 18.0f || y > topY - 18.0f || y < lastY + 24.0f)
                continue;
            const QByteArray text = label.toLatin1();
            renderLabelText(x, y, text.constData(), true);
            lastY = y;
        }
        return;
    }
    const float y = CStavePos(PB_PART_right, MAX_STAVE_INDEX).getPosY() + 26.0f;
    float lastRight = Cfg::scrollStartX() - 1000.0f;
    for (const ChordAnnotation& annotation : m_chordAnnotations)
    {
        const QString label = transposedChordAnnotationLabel(annotation, m_transpose, keySignature);
        if (label.isEmpty())
            continue;
        const float x = Cfg::playZoneX() + viewportTickOffsetPixels(viewport, annotation.startTick);
        const float width = static_cast<float>(label.length()) * 9.0f;
        if (x < Cfg::scrollStartX() || x > Cfg::staveEndX() || x - width / 2.0f < lastRight + 6.0f)
            continue;
        const QByteArray text = label.toLatin1();
        renderLabelText(x, y, text.constData(), false);
        lastRight = x + width / 2.0f;
    }
#else
    Q_UNUSED(viewport);
#endif
}

void CScore::drawScroll(bool refresh)
{
    const ScoreViewport viewport = currentViewport();
    if (viewport.viewMode == PB_VIEW_MODE_synthesia)
    {
        drawSynthesia(refresh, viewport);
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
    drawChordAnnotations(viewport);
    drawScrollingSymbols(viewport, true);
    m_piano->drawPianoInput();
}

void CScore::drawSynthesia(bool refresh, const ScoreViewport& viewport)
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
        drawSynthesiaBeatGuides(leftX, rightX, strikeY, topY, viewport.originTick);
    drawChordAnnotations(viewport);
    drawSynthesiaStrikeLine(leftX, rightX, strikeY);
    drawSynthesiaNotes(viewport);
    drawSynthesiaKeyboard(viewport);
}

qint64 CScore::currentSynthesiaTicks() const
{
    return m_currentTick;
}

bool CScore::showSynthesiaComping() const
{
    return m_settings != nullptr &&
            m_settings->value(SynthesiaCompingViewSetting, false).toBool();
}

const QVector<NoteEvent>& CScore::synthesiaNotes() const
{
    return showSynthesiaComping() ? m_synthesiaCompingNotes : m_noteEvents;
}

bool CScore::synthesiaNoteVisible(const NoteEvent& note) const
{
    return showSynthesiaComping() || noteVisibleForChannel(note, m_activeScroll);
}

void CScore::clearFeedback()
{
    for (int i = 0; i < arraySize(m_feedback); i++)
        m_feedback[i] = ScoreFeedback();
}

const CScore::ScoreFeedback* CScore::feedbackFor(int noteId, int pitch, int slotId) const
{
    if (pitch < 0 || pitch >= arraySize(m_feedback))
        return nullptr;
    const ScoreFeedback& feedback = m_feedback[pitch];
    if (!feedback.active)
        return nullptr;
    if (feedback.noteId >= 0 && feedback.noteId == noteId)
        return &feedback;
    if (feedback.noteId < 0 && feedback.slotId == slotId)
        return &feedback;
    return nullptr;
}

CScore::ScoreFeedbackTarget CScore::feedbackTarget(int pitch, qint64 targetTick) const
{
    ScoreFeedbackTarget target;
    if (m_activeScroll < 0)
        return target;

    const qint64 margin = qMax(1, CMidiFile::getPulsesPerQuarterNote() * 2);
    const QVector<ScoreSlot>& scoreSlots = m_scoreSlots[m_activeScroll];
    const QVector<int> indexes = visibleScoreSlotIndexes(scoreSlots, targetTick - margin, margin * 2);
    qint64 bestDistance = std::numeric_limits<qint64>::max();

    for (int i = 0; i < indexes.size(); i++) {
        const ScoreSlot& slot = scoreSlots[indexes[i]];
        for (int j = 0; j < slot.symbols.size(); j++) {
            if (slot.symbols[j].midiNote != pitch)
                continue;
            const qint64 distance = tickDistance(slot.absoluteTick, targetTick);
            if (distance < bestDistance) {
                bestDistance = distance;
                target.slotId = slot.id;
                target.noteId = slot.symbols[j].noteId;
            }
        }
    }
    return target;
}

void CScore::updateFeedback(int note, PracticeFeedbackKind kind,
                            qint64 wantedDelta, qint64 pianistTimming)
{
    const int pitch = note - m_transpose;
    if (pitch < 0 || pitch >= arraySize(m_feedback))
        return;

    const qint64 targetTick = currentSynthesiaTicks() - deltaAdjustL(wantedDelta);
    const ScoreFeedbackTarget target = feedbackTarget(pitch, targetTick);
    if (!practiceFeedbackActive(kind)) {
        m_feedback[pitch] = ScoreFeedback();
        return;
    }

    m_feedback[pitch].active = true;
    m_feedback[pitch].pitch = pitch;
    m_feedback[pitch].slotId = target.slotId;
    m_feedback[pitch].noteId = target.noteId;
    m_feedback[pitch].kind = kind;
    m_feedback[pitch].timing = pianistTimming == NOT_USED ? NOT_USED :
            deltaAdjustL(pianistTimming) * DEFAULT_PPQN /
            qMax(1, CMidiFile::getPulsesPerQuarterNote());
}

void CScore::setNoteFeedback(int note, PracticeFeedbackKind kind,
                             qint64 wantedDelta, qint64 pianistTimming)
{
    updateFeedback(note, kind, wantedDelta, pianistTimming);
}

void CScore::applySlotFeedback(CSlot *slot, const ScoreSlot& scoreSlot) const
{
    if (slot == nullptr)
        return;
    for (int i = 0; i < slot->length() && i < scoreSlot.symbols.size(); i++) {
        const ScoreSlotSymbol& symbol = scoreSlot.symbols[i];
        const ScoreFeedback *feedback = feedbackFor(symbol.noteId, symbol.midiNote, scoreSlot.id);
        if (feedback == nullptr)
            continue;
        slot->getSymbolPtr(i)->setColor(practiceFeedbackColor(feedback->kind));
        slot->getSymbolPtr(i)->setPianistTiming(feedback->timing);
    }
}

void CScore::collectSynthesiaKeyLights(const ScoreViewport& viewport,
                                       CSynthesiaKeyLight *lights, int lightCount)
{
    const bool compingView = showSynthesiaComping();
    if (lights == nullptr || lightCount <= 0 || (!compingView && m_activeScroll < 0))
        return;

    const float leftX = synthesiaLeftX();
    const float width = synthesiaWhiteKeyWidth();
    const float strikeY = synthesiaStrikeY();
    const float topY = synthesiaTopY();
    const QVector<NoteEvent>& notes = synthesiaNotes();
    const QVector<int> indexes = visibleNoteIndexes(
                notes, viewport.originTick, viewport.visibleTicks);

    for (int i = 0; i < indexes.size(); i++) {
        const NoteEvent& note = notes[indexes[i]];
        if (!synthesiaNoteVisible(note))
            continue;
        const ScoreFeedback *feedback = compingView ? nullptr : feedbackFor(note.id, note.pitch, -1);
        const CColor color = feedback == nullptr ? synthesiaColor(noteHand(note)) :
                practiceFeedbackColor(feedback->kind);
        const SynthesiaNoteRect rect = makeSynthesiaRect(
                    note, viewport, strikeY, topY, leftX, width, color);
        if (noteTouchesStrikeLine(rect, strikeY))
            setKeyLight(lights, lightCount, rect.pitch, rect.color, rect.alpha + 0.08f);
    }
}

void CScore::drawSynthesiaNotes(const ScoreViewport& viewport)
{
    drawSynthesiaNoteBodies(viewport);
    drawSynthesiaNoteLabels(viewport);
}

void CScore::drawSynthesiaNoteBodies(const ScoreViewport& viewport)
{
    const bool compingView = showSynthesiaComping();
    if (!compingView && m_activeScroll < 0)
        return;

    const float leftX = synthesiaLeftX();
    const float width = synthesiaWhiteKeyWidth();
    const float strikeY = synthesiaStrikeY();
    const float topY = synthesiaTopY();
    const QVector<NoteEvent>& notes = synthesiaNotes();
    const QVector<int> indexes = visibleNoteIndexes(
                notes, viewport.originTick, viewport.visibleTicks);

    for (int i = 0; i < indexes.size(); i++) {
        const NoteEvent& note = notes[indexes[i]];
        if (!synthesiaNoteVisible(note))
            continue;
        const ScoreFeedback *feedback = compingView ? nullptr : feedbackFor(note.id, note.pitch, -1);
        const CColor color = feedback == nullptr ? synthesiaColor(noteHand(note)) :
                practiceFeedbackColor(feedback->kind);
        drawSynthesiaNote(makeSynthesiaRect(note, viewport, strikeY, topY, leftX, width, color),
                          strikeY);
    }
}

void CScore::drawSynthesiaNoteLabels(const ScoreViewport& viewport)
{
    const bool compingView = showSynthesiaComping();
    if ((!compingView && m_activeScroll < 0) ||
            m_settings == nullptr || !m_settings->synthesiaNoteNames())
        return;

    const float leftX = synthesiaLeftX();
    const float width = synthesiaWhiteKeyWidth();
    const float strikeY = synthesiaStrikeY();
    const float topY = synthesiaTopY();
    const QVector<NoteEvent>& notes = synthesiaNotes();
    const QVector<int> indexes = visibleNoteIndexes(
                notes, viewport.originTick, viewport.visibleTicks);

    for (int i = 0; i < indexes.size(); i++) {
        const NoteEvent& note = notes[indexes[i]];
        if (!synthesiaNoteVisible(note))
            continue;
        const SynthesiaNoteRect rect = makeSynthesiaRect(
                    note, viewport, strikeY, topY, leftX, width, synthesiaColor(noteHand(note)));
        if (rect.valid)
            drawNoteName(rect.pitch, (rect.left + rect.right) / 2.0f,
                         synthesiaLabelY(rect, strikeY, topY), PB_NOTE_LABEL_synthesia);
    }
}

void CScore::drawSynthesiaKeyboard(const ScoreViewport& viewport)
{
    const int keyCount = KeyboardGeometry::HighestMidiNote - KeyboardGeometry::LowestMidiNote + 1;
    CSynthesiaKeyLight lights[keyCount];
    const float leftX = synthesiaLeftX();
    const float width = synthesiaWhiteKeyWidth();
    const float bottomY = synthesiaKeyboardBottomY();
    const float height = synthesiaKeyboardHeight();

    clearKeyLights(lights, keyCount);
    collectSynthesiaKeyLights(viewport, lights, keyCount);

    drawKeyboardLayer(false, leftX, bottomY, width, height);
    drawChordKeyHighlights(m_piano->getGoodChord(), Cfg::pianoGoodColor(), leftX, bottomY, width, height);
    drawChordKeyHighlights(m_piano->getBadChord(), Cfg::pianoBadColor(), leftX, bottomY, width, height);
    drawSynthesiaKeyLightLayer(lights, keyCount, false, leftX, bottomY, width, height);
    drawKeyboardLayer(true, leftX, bottomY, width, height);
    drawSynthesiaKeyLightLayer(lights, keyCount, true, leftX, bottomY, width, height);
}

void CScore::collectScoreKeyLights(const ScoreViewport& viewport,
                                   CSynthesiaKeyLight *lights, int lightCount) const
{
    if (lights == nullptr || lightCount <= 0 || m_activeScroll < 0)
        return;

    const QVector<ScoreSlot>& scoreSlots = m_scoreSlots[m_activeScroll];
    const QVector<int> indexes = visibleScoreSlotIndexes(
                scoreSlots, viewport.originTick, viewport.visibleTicks);

    for (int i = 0; i < indexes.size(); i++) {
        const ScoreSlot& slot = scoreSlots[indexes[i]];
        bool foundNote = false;
        for (int j = 0; j < slot.symbols.size(); j++) {
            const ScoreSlotSymbol& symbol = slot.symbols[j];
            if (symbol.type < PB_SYMBOL_noteHead)
                continue;
            if (viewport.selectedHand != PB_PART_both && viewport.selectedHand != symbol.hand)
                continue;
            const ScoreFeedback *feedback = feedbackFor(symbol.noteId, symbol.midiNote, slot.id);
            const CColor color = feedback == nullptr ? Cfg::noteColor() :
                    practiceFeedbackColor(feedback->kind);
            setKeyLight(lights, lightCount, symbol.midiNote, color, 0.90f);
            foundNote = true;
        }
        if (foundNote)
            return;
    }
}

void CScore::drawPianoKeyboard()
{
    const int keyCount = KeyboardGeometry::HighestMidiNote - KeyboardGeometry::LowestMidiNote + 1;
    CSynthesiaKeyLight lights[keyCount];
    const float leftX = Cfg::staveStartX();
    const float width = (Cfg::staveEndX() - Cfg::staveStartX()) /
            static_cast<float>(KeyboardGeometry::WhiteKeyCount);
    const float bottomY = 0.0f;
    const float height = 42.0f;

    clearKeyLights(lights, keyCount);
    collectScoreKeyLights(currentViewport(), lights, keyCount);

    drawKeyboardLayer(false, leftX, bottomY, width, height);
    drawChordKeyHighlights(m_piano->getGoodChord(), Cfg::pianoGoodColor(), leftX, bottomY, width, height);
    drawChordKeyHighlights(m_piano->getBadChord(), Cfg::pianoBadColor(), leftX, bottomY, width, height);
    drawSynthesiaKeyLightLayer(lights, keyCount, false, leftX, bottomY, width, height);
    drawKeyboardLayer(true, leftX, bottomY, width, height);
    drawSynthesiaKeyLightLayer(lights, keyCount, true, leftX, bottomY, width, height);
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
