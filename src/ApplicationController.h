#ifndef __APPLICATION_CONTROLLER_H__
#define __APPLICATION_CONTROLLER_H__

#include <QString>
#include <QStringList>
#include <QtGlobal>

#include "Chord.h"
#include "Conductor.h"
#include "MidiDevice.h"

class CSettings;
class CSong;
class CTrackList;

struct ScoreScrollState
{
    qint64 durationTicks = 0;
    qint64 visibleOriginTick = 0;
    qint64 currentTick = 0;
    bool followPlayback = true;
    bool dragging = false;
};

class ApplicationController
{
public:
    ApplicationController(CSong *song, CSettings *settings);

    void openSongFile(const QString& fileName);
    void selectSongName(const QString& songName);
    QString songTitle() const;

    bool playing() const;
    void play(bool start);
    void pause();
    void rewind();
    void playFromStartBar();
    void seekTick(qint64 tick);
    void pcKeyPress(int key, bool down);

    float speed() const;
    void setSpeed(float speed);
    int transpose() const;
    void setTranspose(int semitones);
    void setActiveHand(whichPart_t hand);
    whichPart_t activeHand() const;
    void setActiveChannel(int channel);
    int activeChannel() const;
    void setPlayMode(playMode_t mode);
    playMode_t playMode() const;

    void setBoostVolume(int value);
    void setPianoVolume(int value);
    void setMutePianistPart(bool muted);
    void setTimingMarkers(bool enabled);
    bool timingMarkers() const;
    void setStopPointMode(stopPointMode_t mode);
    stopPointMode_t stopPointMode() const;
    void setRhythmTappingMode(rhythmTapping_t mode);
    rhythmTapping_t rhythmTappingMode() const;

    double currentBarPosition() const;
    double playFromBarPosition() const;
    double loopingBars() const;
    double playUptoBarPosition() const;
    void setPlayFromBarPosition(double bar);
    void setLoopingBars(double bars);

    QStringList midiPortList(CMidiDeviceBase::midiType_t type) const;
    void openMidiPort(CMidiDeviceBase::midiType_t type, const QString& name);
    int latencyFix() const;
    void setLatencyFix(int msec);
    void setPianoSoundPatches(int rightSound, int wrongSound, bool update = false);
    void testWrongNoteSound(bool enabled);
    void reconnectMidi();
    void flushMidiInput();

    CTrackList* trackList() const;
    void initializeTrackList(CTrackList *trackList);
    void selectTrackRow(int row);
    void setTrackHands(int leftRow, int rightRow);
    void invalidateActiveScoreCache();
    void invalidateScoreRendererCaches();
    void rebuildScoreData();
    void rebuildPlaybackEvents();
    void updateAnnotatedChordPlaybackVolume();
    void forceScoreRedraw();
    void regenerateChordTimeline();

    ScoreScrollState scrollbarState() const;
    void beginScrollbarDrag();
    void updateScrollbarDrag(qint64 tick);
    void finishScrollbarDrag(qint64 tick);
    void clickScrollbarSeek(qint64 tick);
    void setScrollbarFollowPlayback(bool follow);

private:
    qint64 boundedSongTick(qint64 tick) const;
    qint64 tickAtBarPosition(double bar) const;

    CSong *m_song;
    CSettings *m_settings;
    bool m_scrollbarDragging;
    bool m_scrollbarFollowPlayback;
    qint64 m_scrollbarVisibleOriginTick;
};

#endif
