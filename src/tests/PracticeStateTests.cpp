#include <iostream>

#include "PracticeState.h"

namespace {
int failures = 0;

void expectInt(const char *name, int actual, int expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

CChord chordWith(int pitch)
{
    CChord chord;
    chord.addNote(PB_PART_right, pitch);
    return chord;
}

void testSaveAndTake()
{
    PracticeState state;
    clearPracticeState(&state);
    saveRhythmChord(&state, 60, chordWith(72));

    CChord chord = takeRhythmChord(&state, 60);
    expectInt("saved chord length", chord.length(), 1);
    expectInt("saved chord pitch", chord.getNote(0).pitch(), 72);
    expectInt("taken chord clears", takeRhythmChord(&state, 60).length(), 0);
}

void testUnknownKey()
{
    PracticeState state;
    clearPracticeState(&state);
    saveRhythmChord(&state, 60, chordWith(72));

    expectInt("unknown key", takeRhythmChord(&state, 61).length(), 0);
}
}

int main()
{
    testSaveAndTake();
    testUnknownKey();

    if (failures == 0) {
        std::cout << "PracticeState tests passed\n";
        return 0;
    }
    std::cerr << failures << " practice state test(s) failed\n";
    return 1;
}
