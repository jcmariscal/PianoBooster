/*********************************************************************************/
/*!
@file           KeyboardGeometry.h

@brief          Shared 88-key piano geometry helpers.

    Copyright (c)   2026, PianoBooster contributors, all rights reserved

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

#ifndef __KEYBOARD_GEOMETRY_H__
#define __KEYBOARD_GEOMETRY_H__

namespace KeyboardGeometry
{
constexpr int LowestMidiNote = 21;
constexpr int HighestMidiNote = 108;
constexpr int WhiteKeyCount = 52;

inline int pitchClass(int midiNote)
{
    const int value = midiNote % 12;
    return value < 0 ? value + 12 : value;
}

inline bool isBlackKey(int midiNote)
{
    const int note = pitchClass(midiNote);
    return note == 1 || note == 3 || note == 6 || note == 8 || note == 10;
}

inline int clampMidiNote(int midiNote)
{
    if (midiNote < LowestMidiNote)
        return LowestMidiNote;
    if (midiNote > HighestMidiNote)
        return HighestMidiNote;
    return midiNote;
}

inline int whiteKeyCountBefore(int midiNote)
{
    int count = 0;
    const int lastNote = clampMidiNote(midiNote);
    for (int note = LowestMidiNote; note < lastNote; ++note)
        if (!isBlackKey(note))
            count++;
    return count;
}

inline float keyLeft(int midiNote, float leftX, float whiteKeyWidth)
{
    const int note = clampMidiNote(midiNote);
    const float whiteIndex = static_cast<float>(whiteKeyCountBefore(note));
    if (isBlackKey(note))
        return leftX + (whiteIndex - 0.32f) * whiteKeyWidth;
    return leftX + whiteIndex * whiteKeyWidth;
}

inline float keyWidth(int midiNote, float whiteKeyWidth)
{
    return isBlackKey(midiNote) ? whiteKeyWidth * 0.62f : whiteKeyWidth * 0.92f;
}

inline float keyboardRight(float leftX, float whiteKeyWidth)
{
    return leftX + static_cast<float>(WhiteKeyCount) * whiteKeyWidth;
}
}

#endif // __KEYBOARD_GEOMETRY_H__
