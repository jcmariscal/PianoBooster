#include "ChordTimeline.h"

#include <algorithm>
#include <limits>

namespace {
int safeDelta(qint64 previousTick, qint64 tick)
{
    qint64 delta = tick - previousTick;
    if (delta < 0)
        delta = 0;
    if (delta > std::numeric_limits<int>::max())
        return std::numeric_limits<int>::max();
    return static_cast<int>(delta);
}

CMidiEvent timelineEvent(const MidiEventRecord& record, qint64 previousTick)
{
    CMidiEvent event = record.event;
    event.setDeltaTime(safeDelta(previousTick, record.absoluteTick));
    return event;
}

void appendFoundChord(ChordTimeline *timeline, CFindChord *finder, qint64 *tick)
{
    CChord chord = finder->getChord();
    *tick += chord.getDeltaTime();
    TimelineChord entry;
    entry.chord = chord;
    entry.tick = *tick;
    timeline->chords.append(entry);
}
}

ChordTimeline buildChordTimeline(const SongData& song, int channel, whichPart_t part)
{
    ChordTimeline timeline;
    CFindChord finder;
    qint64 previousTick = 0;
    qint64 chordTick = 0;

    for (int i = 0; i < song.events.size(); i++) {
        const MidiEventRecord& record = song.events[i];
        CMidiEvent event = timelineEvent(record, previousTick);
        previousTick = record.absoluteTick;
        if (finder.findChord(event, channel, part))
            appendFoundChord(&timeline, &finder, &chordTick);
    }
    return timeline;
}

int chordTimelineIndexAt(const ChordTimeline& timeline, qint64 tick)
{
    if (tick < 0)
        tick = 0;
    auto it = std::lower_bound(timeline.chords.begin(), timeline.chords.end(), tick,
                               [](const TimelineChord& chord, qint64 value) {
        return chord.tick < value;
    });
    return static_cast<int>(it - timeline.chords.begin());
}
