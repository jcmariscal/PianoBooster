/*********************************************************************************/
/*!
@file           Chord.cpp

@brief          Reads ahead from the songdata and collects chords to be matched.

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

#include "Chord.h"
#include "Cfg.h"

#include <QHash>
#include <QString>

#include <algorithm>
#include <cmath>

int CNote::m_leftHandChannel = -2;
int CNote::m_rightHandChannel = -2;
int CNote::m_rightHandTrack[MAX_MIDI_CHANNELS];
bool CNote::m_splitHandChannel[MAX_MIDI_CHANNELS];
bool CNote::m_splitHands = false;
splitHandsMode_t CNote::m_splitHandsMode = PB_SPLIT_HANDS_naive;

whichPart_t CNote::m_activeHand = PB_PART_both;

int CChord::m_cfg_highestPianoNote = 127; // The highest note on the users piano keyboard;
int CChord::m_cfg_lowestPianoNote = 0;

namespace {

struct SplitActiveNote
{
    int pitch;
    int offset;
};

struct SplitState
{
    QVector<SplitActiveNote> left;
    QVector<SplitActiveNote> right;
    int lastPitchLeft = -1;
    int lastTimeLeft = 0;
    int lastPitchRight = -1;
    int lastTimeRight = 0;
};

struct SplitPath
{
    SplitState state;
    double cost = 0.0;
    int previous = -1;
    int mask = 0;
    int rightCount = 0;
};

struct SplitEvent
{
    QVector<int> notes;
    int onset = 0;
};

QHash<QString, whichPart_t> g_splitHandAssignments;

QString splitNoteKey(int onset, int channel, int track, int pitch)
{
    return QString::number(onset) + ':' + QString::number(channel) + ':' +
            QString::number(track) + ':' + QString::number(pitch);
}

QString splitNoteKey(const CSplitHandNote& note)
{
    return splitNoteKey(note.onset, note.channel, note.track, note.pitch);
}

int noteSpan(const QVector<int>& pitches)
{
    if (pitches.count() <= 1)
        return 0;
    return pitches.last() - pitches.first();
}

bool validPartition(const QVector<int>& eventNotes, int mask, const QVector<CSplitHandNote>& notes)
{
    QVector<int> left;
    QVector<int> right;
    for (int i = 0; i < eventNotes.count(); ++i)
    {
        if (mask & (1 << i))
            right.append(notes[eventNotes[i]].pitch);
        else
            left.append(notes[eventNotes[i]].pitch);
    }
    std::sort(left.begin(), left.end());
    std::sort(right.begin(), right.end());
    return noteSpan(left) <= 14 && noteSpan(right) <= 14;
}

QVector<int> partitionMasks(const SplitEvent& event, const QVector<CSplitHandNote>& notes)
{
    QVector<int> masks;
    const int count = event.notes.count();
    const int maxMask = (count <= 12) ? (1 << count) : 0;
    if (maxMask > 0) {
        for (int mask = 0; mask < maxMask; ++mask)
            if (validPartition(event.notes, mask, notes))
                masks.append(mask);
        if (!masks.isEmpty())
            return masks;
    }
    for (int split = 0; split <= count && count < 31; ++split) {
        int mask = 0;
        for (int i = split; i < count; ++i)
            mask |= 1 << i;
        if (validPartition(event.notes, mask, notes))
            masks.append(mask);
    }
    if (masks.isEmpty()) {
        int mask = 0;
        for (int i = 0; i < count && i < 31; ++i)
            if (notes[event.notes[i]].pitch >= MIDDLE_C)
                mask |= 1 << i;
        masks.append(mask);
    }
    return masks;
}

void sortActive(QVector<SplitActiveNote>& active)
{
    std::sort(active.begin(), active.end(), [](const SplitActiveNote& a, const SplitActiveNote& b) {
        return a.pitch == b.pitch ? a.offset < b.offset : a.pitch < b.pitch;
    });
}

void releaseHand(QVector<SplitActiveNote>& active, int onset, int& lastPitch, int& lastTime)
{
    QVector<SplitActiveNote> held;
    for (const SplitActiveNote& note : active) {
        if (note.offset <= onset) {
            if (note.offset > lastTime || (note.offset == lastTime && note.pitch > lastPitch)) {
                lastPitch = note.pitch;
                lastTime = note.offset;
            }
        } else {
            held.append(note);
        }
    }
    active = held;
}

SplitState releaseState(SplitState state, int onset)
{
    releaseHand(state.left, onset, state.lastPitchLeft, state.lastTimeLeft);
    releaseHand(state.right, onset, state.lastPitchRight, state.lastTimeRight);
    return state;
}

void addPartitionNotes(SplitState& state, const SplitEvent& event, int mask, const QVector<CSplitHandNote>& notes)
{
    for (int i = 0; i < event.notes.count(); ++i) {
        const CSplitHandNote& note = notes[event.notes[i]];
        SplitActiveNote active = {note.pitch, note.offset};
        if (mask & (1 << i))
            state.right.append(active);
        else
            state.left.append(active);
    }
    sortActive(state.left);
    sortActive(state.right);
}

int activeSpan(const QVector<SplitActiveNote>& active)
{
    if (active.count() <= 1)
        return 0;
    int low = active.first().pitch;
    int high = active.first().pitch;
    for (const SplitActiveNote& note : active) {
        low = std::min(low, note.pitch);
        high = std::max(high, note.pitch);
    }
    return high - low;
}

double activeCost(const QVector<SplitActiveNote>& active)
{
    double cost = 0.0;
    const int span = activeSpan(active);
    if (span > 14)
        cost += 50.0 * (span - 14) * (span - 14);
    if (span > 16)
        cost += 1000.0;
    if (active.count() > 5)
        cost += 1000.0;
    return cost;
}

double crossoverCost(const SplitState& state)
{
    if (state.left.isEmpty() || state.right.isEmpty())
        return 0.0;
    const int overlap = state.left.last().pitch - state.right.first().pitch;
    return (overlap > 0) ? 5.0 * overlap : 0.0;
}

double collisionCost(const SplitState& state)
{
    int left = 0;
    int right = 0;
    double cost = 0.0;
    while (left < state.left.count() && right < state.right.count()) {
        if (state.left[left].pitch == state.right[right].pitch) {
            cost += 500.0;
            left++;
            right++;
        } else if (state.left[left].pitch < state.right[right].pitch) {
            left++;
        } else {
            right++;
        }
    }
    return cost;
}

double jumpCost(int pitch, int onset, int lastPitch, int lastTime)
{
    if (lastPitch < 0)
        return 0.0;
    const int jump = std::abs(pitch - lastPitch);
    const int minGap = std::max(1, CMidiFile::getPulsesPerQuarterNote() / 20);
    const int gap = std::max(minGap, onset - lastTime);
    return 2.0 * jump * jump * CMidiFile::getPulsesPerQuarterNote() / gap;
}

double partitionTransitionCost(const SplitState& released, const SplitState& next,
                               const SplitEvent& event, int mask, const QVector<CSplitHandNote>& notes)
{
    double cost = activeCost(next.left) + activeCost(next.right) + crossoverCost(next);
    cost += collisionCost(next);
    for (int i = 0; i < event.notes.count(); ++i) {
        const CSplitHandNote& note = notes[event.notes[i]];
        if (mask & (1 << i)) {
            cost += jumpCost(note.pitch, note.onset, released.lastPitchRight, released.lastTimeRight);
            if (event.notes.count() == 1 && released.right.isEmpty() && released.left.isEmpty() &&
                    released.lastPitchLeft >= 0 && std::abs(note.pitch - released.lastPitchLeft) <= 12)
                cost += 100.0;
            if (released.lastPitchLeft >= 0 && std::abs(note.pitch - released.lastPitchLeft) <= 3)
                cost += 1.0;
        } else {
            cost += jumpCost(note.pitch, note.onset, released.lastPitchLeft, released.lastTimeLeft);
            if (event.notes.count() == 1 && released.left.isEmpty() && released.right.isEmpty() &&
                    released.lastPitchRight >= 0 && std::abs(note.pitch - released.lastPitchRight) <= 12)
                cost += 100.0;
            if (released.lastPitchRight >= 0 && std::abs(note.pitch - released.lastPitchRight) <= 3)
                cost += 1.0;
        }
    }
    return cost;
}

QString activeKey(const QVector<SplitActiveNote>& active)
{
    QString key;
    for (const SplitActiveNote& note : active)
        key += QString::number(note.pitch) + ',' + QString::number(note.offset) + ';';
    return key;
}

QString stateKey(const SplitState& state)
{
    return activeKey(state.left) + '|' + activeKey(state.right) + '|' +
            QString::number(state.lastPitchLeft) + ',' + QString::number(state.lastTimeLeft) + '|' +
            QString::number(state.lastPitchRight) + ',' + QString::number(state.lastTimeRight);
}

bool betterPath(const SplitPath& a, const SplitPath& b)
{
    if (std::abs(a.cost - b.cost) > 0.000001)
        return a.cost < b.cost;
    return a.rightCount > b.rightCount;
}

int bitCount(int value)
{
    int count = 0;
    while (value) {
        count += value & 1;
        value >>= 1;
    }
    return count;
}

void addCandidate(QVector<SplitPath>& candidates, QHash<QString, int>& indexByState, const SplitPath& path)
{
    const QString key = stateKey(path.state);
    if (!indexByState.contains(key)) {
        indexByState.insert(key, candidates.count());
        candidates.append(path);
    } else {
        int index = indexByState.value(key);
        if (betterPath(path, candidates[index]))
            candidates[index] = path;
    }
}

QVector<SplitEvent> makeSplitEvents(const QVector<CSplitHandNote>& notes)
{
    QVector<int> order;
    for (int i = 0; i < notes.count(); ++i)
        order.append(i);
    std::sort(order.begin(), order.end(), [&notes](int a, int b) {
        return notes[a].onset == notes[b].onset ? notes[a].pitch < notes[b].pitch : notes[a].onset < notes[b].onset;
    });

    QVector<SplitEvent> events;
    const int window = std::max(1, CMidiFile::getPulsesPerQuarterNote() / 24);
    for (int idx : order) {
        if (events.isEmpty() || notes[idx].onset - events.last().onset > window)
            events.append(SplitEvent());
        if (events.last().notes.isEmpty())
            events.last().onset = notes[idx].onset;
        events.last().notes.append(idx);
    }
    return events;
}

QVector<SplitPath> pruneBeam(QVector<SplitPath> paths, int beamSize)
{
    std::sort(paths.begin(), paths.end(), betterPath);
    if (paths.count() > beamSize)
        paths.resize(beamSize);
    return paths;
}

QVector<whichPart_t> backtrackLabels(const QVector<QVector<SplitPath>>& layers,
                                     const QVector<SplitEvent>& events, int noteCount)
{
    QVector<whichPart_t> labels(noteCount, PB_PART_right);
    int stateIndex = 0;
    for (int i = layers.count() - 1; i >= 0 && stateIndex >= 0; --i) {
        const SplitPath& path = layers[i][stateIndex];
        for (int j = 0; j < events[i].notes.count(); ++j)
            labels[events[i].notes[j]] = (path.mask & (1 << j)) ? PB_PART_right : PB_PART_left;
        stateIndex = path.previous;
    }
    return labels;
}

void applyGraceNotes(QVector<whichPart_t>& labels, const QVector<CSplitHandNote>& notes)
{
    const int graceTicks = std::max(1, CMidiFile::getPulsesPerQuarterNote() / 16);
    for (int i = 0; i < notes.count(); ++i) {
        if (notes[i].offset - notes[i].onset >= graceTicks)
            continue;
        int next = -1;
        for (int j = 0; j < notes.count(); ++j)
            if (notes[j].onset >= notes[i].offset && (next < 0 || notes[j].onset < notes[next].onset))
                next = j;
        if (next >= 0)
            labels[i] = labels[next];
    }
}

QVector<whichPart_t> costSplitHands(const QVector<CSplitHandNote>& notes, int beamSize)
{
    const QVector<SplitEvent> events = makeSplitEvents(notes);
    QVector<QVector<SplitPath>> layers;
    QVector<SplitPath> previous;
    for (int i = 0; i < events.count(); ++i) {
        QVector<SplitPath> candidates;
        QHash<QString, int> indexByState;
        for (int mask : partitionMasks(events[i], notes)) {
            const int sourceCount = (i == 0) ? 1 : previous.count();
            for (int p = 0; p < sourceCount; ++p) {
                SplitState released = (i == 0) ? SplitState() : releaseState(previous[p].state, events[i].onset);
                SplitPath path;
                path.state = released;
                addPartitionNotes(path.state, events[i], mask, notes);
                path.cost = ((i == 0) ? 0.0 : previous[p].cost) +
                        partitionTransitionCost(released, path.state, events[i], mask, notes);
                path.previous = (i == 0) ? -1 : p;
                path.mask = mask;
                path.rightCount = ((i == 0) ? 0 : previous[p].rightCount) + bitCount(mask);
                addCandidate(candidates, indexByState, path);
            }
        }
        previous = pruneBeam(candidates, beamSize);
        layers.append(previous);
    }
    QVector<whichPart_t> labels = backtrackLabels(layers, events, notes.count());
    applyGraceNotes(labels, notes);
    return labels;
}

} // namespace


void CNote::reset()
{
    CNote::setChannelHands(-2, -2);  // -2 for not set -1 for none

    for (int chan = 0; chan < MAX_MIDI_CHANNELS; chan++) {
        m_rightHandTrack[chan]=-1;
    }
    clearSplitHandChannels();
    clearSplitHandAssignments();
}

void CNote::setChannelHands(int left, int right)
{
    m_leftHandChannel = left;
    m_rightHandChannel = right;
}

void CNote::clearSplitHandChannels()
{
    for (int chan = 0; chan < MAX_MIDI_CHANNELS; chan++)
        m_splitHandChannel[chan] = false;
}

void CNote::setSplitHandChannel(int channel, bool enabled)
{
    if (channel < 0 || channel >= MAX_MIDI_CHANNELS)
        return;
    m_splitHandChannel[channel] = enabled;
}

void CNote::clearSplitHandAssignments()
{
    g_splitHandAssignments.clear();
}

void CNote::assignSplitHands(const QVector<CSplitHandNote>& notes)
{
    clearSplitHandAssignments();
    if (notes.isEmpty() || CNote::splitHandsMode() != PB_SPLIT_HANDS_cost)
        return;

    const QVector<whichPart_t> labels = costSplitHands(notes, 50);
    for (int i = 0; i < notes.count(); ++i)
        g_splitHandAssignments.insert(splitNoteKey(notes[i]), labels[i]);
}

int CNote::splitPointForPitches(const int *pitches, int count)
{
    if (pitches == nullptr || count <= 0)
        return MIDDLE_C;

    int sorted[MAX_MIDI_NOTES];
    if (count > arraySize(sorted))
        count = arraySize(sorted);

    bool hasBelowMiddleC = false;
    bool hasAtOrAboveMiddleC = false;
    for (int i = 0; i < count; ++i)
    {
        sorted[i] = pitches[i];
        if (pitches[i] < MIDDLE_C)
            hasBelowMiddleC = true;
        else
            hasAtOrAboveMiddleC = true;
    }

    if (!hasBelowMiddleC || !hasAtOrAboveMiddleC)
        return MIDDLE_C;

    std::sort(sorted, sorted + count);

    int bestSplitPoint = MIDDLE_C;
    int bestScore = -1000000;
    for (int i = 0; i + 1 < count; ++i)
    {
        if (sorted[i] == sorted[i + 1])
            continue;

        const int candidate = (sorted[i] + sorted[i + 1] + 1) / 2;
        if (candidate < MIDDLE_C - MIDI_OCTAVE || candidate > MIDDLE_C + MIDI_OCTAVE)
            continue;

        const int gap = sorted[i + 1] - sorted[i];
        const int distanceFromMiddleC = (candidate > MIDDLE_C) ? candidate - MIDDLE_C : MIDDLE_C - candidate;
        const int score = gap * 4 - distanceFromMiddleC;

        if (score > bestScore)
        {
            bestScore = score;
            bestSplitPoint = candidate;
        }
    }

    return bestSplitPoint;
}

whichPart_t CNote::findHand(CMidiEvent midi, int whichChannel, whichPart_t whichPart)
{
    int midiNote = midi.note();
    int midiChannel = midi.channel();
    whichPart_t hand = PB_PART_none;
    // exit if it is not for this channel
    if (midiChannel != whichChannel)
    {
        // return none if this is not being used with the other hand.
        if (!(CNote::splitHandsForChannel(whichChannel) && CNote::splitHandsForChannel(midiChannel)) &&
                (CNote::hasPianoPart(whichChannel) == false || CNote::hasPianoPart(midiChannel) == false))
            return PB_PART_none;
    }
    int rightHandTrack = CNote::rightHandTrack(midiChannel);

    const bool sameHandChannel = midiChannel == CNote::leftHandChan() &&
            midiChannel == CNote::rightHandChan();

    if (CNote::splitHandsForChannel(whichChannel) && CNote::splitHandsForChannel(midiChannel)) {
        hand = g_splitHandAssignments.value(
                    splitNoteKey(midi.absoluteTime(), midi.channel(), midi.track(), midi.originalNote()),
                    CNote::splitHandForPitch(midiNote, MIDDLE_C));
    } else if (midiChannel == whichChannel && rightHandTrack >= 0 ) {
        hand = (midi.track() == rightHandTrack) ? PB_PART_right : PB_PART_left ;
    } else if (sameHandChannel) {
        hand = CNote::splitHandForPitch(midiNote, MIDDLE_C);
    } else if (midiChannel == CNote::rightHandChan()) {
        hand  = PB_PART_right;
    } else if (midiChannel == CNote::leftHandChan()) {
        hand  = PB_PART_left;
    } else  if (midiChannel == whichChannel) {
        hand = (midiNote >= MIDDLE_C) ? PB_PART_right : PB_PART_left ;
    }

    if (whichPart == PB_PART_left && hand == PB_PART_right)
        hand = PB_PART_none;
    if (whichPart == PB_PART_right && hand == PB_PART_left)
        hand = PB_PART_none;
    return hand;
}

void CChord::addNote(whichPart_t part, int note, int duration)
{
    if (m_length >= MAX_CHORD_NOTES)
    {
        ppDEBUG(("Over the chord note limit"));
        return;
    }

    if (searchChord(note)) // don't add duplicates
        return;
    m_notes[m_length] = CNote(part, note, duration);
    m_length++;
}

void CChord::applySplitHands()
{
    if (!CNote::splitHandsEnabled() || m_length == 0)
        return;

    int pitches[MAX_CHORD_NOTES];
    for (int i = 0; i < m_length; ++i)
        pitches[i] = m_notes[i].pitch();

    const int splitPoint = CNote::splitPointForPitches(pitches, m_length);
    for (int i = 0; i < m_length; ++i)
        m_notes[i].setPart(CNote::splitHandForPitch(m_notes[i].pitch(), splitPoint));
}


//////////////// CChord /////////////////////

bool CChord::removeNote(int note)
{
    int i;
    bool noteFound = false;

    for (i = 0; i < MAX_CHORD_NOTES; i++)
    {
        if (i >= m_length)
            break;
        if (noteFound == false)
        {
            if (getNote(i).pitch() == note)
                noteFound = true;
        }
        else
        {
            // shove everything else up to remove the note
            m_notes[i-1] = getNote(i);
        }
    }
    if (noteFound)
        m_length--;
    return noteFound;
}

bool CChord::searchChord(int note, int transpose)
{
    int i;

    for (i = 0; i < m_length; i++)
    {
        if (getNote(i).pitch() + transpose == note)
            return true;
    }
    return false;
}

int CChord::trimOutOfRangeNotes(int transpose)
{
    int i;
    int removedNotes = 0;

    for (i = 0; i < MAX_CHORD_NOTES; i++)
    {
        if (i >= m_length)
            break;

        if (!isNotePlayable(getNote(i).pitch(), transpose) || !isHandPlayable(getNote(i).part()))
            removedNotes++; // remove the note
        else if (removedNotes)
        {
            // shove everything else up to remove the note
            m_notes[i-removedNotes] = getNote(i);
        }
    }
    m_length -= removedNotes;
    return m_length;
}

bool CFindChord::findChord(CMidiEvent midi, int channel, whichPart_t part)
{
    bool foundChord = false;

    if (midi.type() == MIDI_PB_EOF)
    {
        if (m_currentChord.length() > 0)
        {
            if (CNote::splitHandsNaive() && CNote::splitHandsForChannel(channel))
                m_currentChord.applySplitHands();
            m_completeChord = m_currentChord;
            foundChord = true;
        }
        return foundChord;
    }

    m_noteGapTime += midi.deltaTime();

    if ((m_noteGapTime >= m_cfg_ChordNoteGap || m_cordSpanGapTime > m_cfg_ChordMaxLength)
            && m_currentChord.length() > 0 )
    {
        foundChord = true;
        if (CNote::splitHandsNaive() && CNote::splitHandsForChannel(channel))
            m_currentChord.applySplitHands();
        m_completeChord = m_currentChord;
        m_currentChord.clear();
    }

    if (midi.type() == MIDI_NOTE_ON)
    {
        whichPart_t hand = CNote::findHand( midi, channel, part );
        if ( hand != PB_PART_none)
        {
            m_currentChord.addNote(hand, midi.note());
            m_currentChord.setDeltaTime(m_noteGapTime + m_currentChord.getDeltaTime());
            if (m_currentChord.length() <= 1)
                m_cordSpanGapTime = 0;
            else
                m_cordSpanGapTime += m_noteGapTime; // measure the span of the cord
            m_noteGapTime = 0;
        }
    }
    return foundChord;
}
