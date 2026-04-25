#ifndef __PRACTICE_ENGINE_H__
#define __PRACTICE_ENGINE_H__

#include "Chord.h"
#include "MidiEvent.h"
#include "PracticeFeedback.h"
#include "StavePosition.h"

enum PracticeRatingEventType
{
    PracticeRatingNone,
    PracticeRatingTotalNotes,
    PracticeRatingWrongNotes,
    PracticeRatingLateNotes
};

struct PracticeRatingEvent
{
    PracticeRatingEventType type = PracticeRatingNone;
    int count = 0;
};

bool practiceNoteMatchesChord(CChord chord, const CMidiEvent& inputNote,
                              int transpose, qint64 chordDeltaTime,
                              qint64 playZoneEarly);
bool practiceChordComplete(int goodNoteCount, int wantedNoteCount,
                           int badNoteCount, int skill);
PracticeFeedbackKind playedNoteFeedback(bool followTimeout);
PracticeFeedbackKind releasedNoteFeedback(bool followTimeout);
qint64 practiceTimingMarker(bool timingMarkers, bool advanced,
                            int playMode, int rhythmMode, qint64 timing);
PracticeRatingEvent totalNotesEvent(int count);
PracticeRatingEvent wrongNotesEvent(int count);
PracticeRatingEvent lateNotesEvent(int count);

#endif
