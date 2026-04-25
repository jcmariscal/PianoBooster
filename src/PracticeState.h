#ifndef __PRACTICE_STATE_H__
#define __PRACTICE_STATE_H__

#include "Chord.h"

constexpr int PracticeSavedRhythmChordCount = 20;

struct PracticeSavedRhythmChord
{
    int key = 0;
    CChord chord;
};

struct PracticeState
{
    PracticeSavedRhythmChord savedRhythmChords[PracticeSavedRhythmChordCount];
};

void clearPracticeState(PracticeState *state);
void saveRhythmChord(PracticeState *state, int key, CChord chord);
CChord takeRhythmChord(PracticeState *state, int key);

#endif
