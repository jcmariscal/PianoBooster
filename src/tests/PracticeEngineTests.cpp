#include <iostream>

#include "PracticeEngine.h"

namespace {
int failures = 0;

void expectBool(const char *name, bool actual, bool expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

void expectInt(const char *name, int actual, int expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

CMidiEvent noteOn(int pitch)
{
    CMidiEvent event;
    event.noteOnEvent(0, 0, pitch, 90);
    return event;
}

CChord chordWith(int pitch)
{
    CChord chord;
    chord.addNote(PB_PART_right, pitch);
    return chord;
}

void testNoteMatching()
{
    expectBool("matching note", practiceNoteMatchesChord(chordWith(60), noteOn(60), 0, 0, 10), true);
    expectBool("early note rejected", practiceNoteMatchesChord(chordWith(60), noteOn(60), 0, -11, 10), false);
    expectBool("wrong note rejected", practiceNoteMatchesChord(chordWith(60), noteOn(61), 0, 0, 10), false);
}

void testChordComplete()
{
    expectBool("advanced incomplete", practiceChordComplete(1, 2, 0, 3), false);
    expectBool("advanced complete", practiceChordComplete(2, 2, 0, 3), true);
    expectBool("beginner one note", practiceChordComplete(1, 3, 0, 2), true);
    expectBool("bad notes block", practiceChordComplete(3, 3, 2, 5), false);
}

void testFeedback()
{
    expectInt("played good", playedNoteFeedback(false), PracticeFeedbackGood);
    expectInt("played bad", playedNoteFeedback(true), PracticeFeedbackBad);
    expectInt("release clear", releasedNoteFeedback(false), PracticeFeedbackPending);
    expectInt("release stopped", releasedNoteFeedback(true), PracticeFeedbackStopped);
}
}

int main()
{
    testNoteMatching();
    testChordComplete();
    testFeedback();

    if (failures == 0) {
        std::cout << "PracticeEngine tests passed\n";
        return 0;
    }
    std::cerr << failures << " practice engine test(s) failed\n";
    return 1;
}
