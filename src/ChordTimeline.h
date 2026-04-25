#ifndef __CHORD_TIMELINE_H__
#define __CHORD_TIMELINE_H__

#include <QVector>

#include "Chord.h"
#include "SongData.h"

struct TimelineChord
{
    CChord chord;
    qint64 tick = 0;
};

struct ChordTimeline
{
    QVector<TimelineChord> chords;
};

ChordTimeline buildChordTimeline(const SongData& song, int channel, whichPart_t part);
int chordTimelineIndexAt(const ChordTimeline& timeline, qint64 tick);

#endif
