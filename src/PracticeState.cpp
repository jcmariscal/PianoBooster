#include "PracticeState.h"

#include "Util.h"

void clearPracticeState(PracticeState *state)
{
    if (state == nullptr)
        return;
    for (int i = 0; i < arraySize(state->savedRhythmChords); i++)
        state->savedRhythmChords[i] = PracticeSavedRhythmChord();
}

void saveRhythmChord(PracticeState *state, int key, CChord chord)
{
    if (state == nullptr)
        return;
    for (int i = 0; i < arraySize(state->savedRhythmChords); i++) {
        if (state->savedRhythmChords[i].key == 0) {
            state->savedRhythmChords[i].key = key;
            state->savedRhythmChords[i].chord = chord;
            return;
        }
    }
    state->savedRhythmChords[0].key = key;
    state->savedRhythmChords[0].chord = chord;
}

CChord takeRhythmChord(PracticeState *state, int key)
{
    CChord empty;
    if (state == nullptr)
        return empty;
    for (int i = 0; i < arraySize(state->savedRhythmChords); i++) {
        if (state->savedRhythmChords[i].key == key) {
            CChord chord = state->savedRhythmChords[i].chord;
            state->savedRhythmChords[i] = PracticeSavedRhythmChord();
            return chord;
        }
    }
    return empty;
}
