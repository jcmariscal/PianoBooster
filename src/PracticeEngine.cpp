#include "PracticeEngine.h"

bool practiceNoteMatchesChord(CChord chord, const CMidiEvent& inputNote,
                              int transpose, qint64 chordDeltaTime,
                              qint64 playZoneEarly)
{
    if (chordDeltaTime <= -playZoneEarly)
        return false;
    return chord.searchChord(inputNote.note(), transpose);
}

bool practiceChordComplete(int goodNoteCount, int wantedNoteCount,
                           int badNoteCount, int skill)
{
    if (badNoteCount >= 2)
        return false;
    if (skill >= 3)
        return goodNoteCount == wantedNoteCount;
    return goodNoteCount >= 1;
}

PracticeFeedbackKind playedNoteFeedback(bool followTimeout)
{
    return followTimeout ? PracticeFeedbackBad : PracticeFeedbackGood;
}

PracticeFeedbackKind releasedNoteFeedback(bool followTimeout)
{
    return followTimeout ? PracticeFeedbackStopped : PracticeFeedbackPending;
}

qint64 practiceTimingMarker(bool timingMarkers, bool advanced,
                            int playMode, int rhythmMode, qint64 timing)
{
    if ((timingMarkers && advanced) || playMode == rhythmMode)
        return timing;
    return NOT_USED;
}

PracticeRatingEvent totalNotesEvent(int count)
{
    return {PracticeRatingTotalNotes, count};
}

PracticeRatingEvent wrongNotesEvent(int count)
{
    return {PracticeRatingWrongNotes, count};
}

PracticeRatingEvent lateNotesEvent(int count)
{
    return {PracticeRatingLateNotes, count};
}
