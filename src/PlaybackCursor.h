#ifndef __PLAYBACK_CURSOR_H__
#define __PLAYBACK_CURSOR_H__

#include "SongData.h"

struct PlaybackCursor
{
    const QVector<MidiEventRecord>* events = nullptr;
    int index = 0;
    qint64 tick = 0;
};

void setPlaybackCursorEvents(PlaybackCursor& cursor,
                             const QVector<MidiEventRecord>* events);
void seekPlaybackCursor(PlaybackCursor& cursor, qint64 tick);
bool readPlaybackCursorEvent(PlaybackCursor& cursor, CMidiEvent& event);
int playbackCursorIndex(const PlaybackCursor& cursor);

#endif
