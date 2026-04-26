#include "ApplicationController.h"

#include "BarMap.h"
#include "Settings.h"
#include "Song.h"
#include "TrackList.h"

ApplicationController::ApplicationController(CSong *song, CSettings *settings)
    : m_song(song),
      m_settings(settings),
      m_scrollbarDragging(false),
      m_scrollbarFollowPlayback(true),
      m_scrollbarVisibleOriginTick(0)
{
}

void ApplicationController::openSongFile(const QString& fileName)
{
    if (m_settings != nullptr)
        m_settings->openSongFile(fileName);
}

void ApplicationController::selectSongName(const QString& songName)
{
    if (m_song == nullptr || m_settings == nullptr)
        return;
    m_settings->setCurrentSongName(songName);
    const QString fileName = m_settings->getCurrentSongLongFileName();
    if (fileName.isEmpty())
        return;
    m_song->loadSong(fileName);
    m_settings->applyLoadedSongSettings(m_song->getSongTitle());
}

QString ApplicationController::songTitle() const
{
    return m_song == nullptr ? QString() : m_song->getSongTitle();
}

bool ApplicationController::playing() const
{
    return m_song != nullptr && m_song->playingMusic();
}

void ApplicationController::play(bool start)
{
    if (m_song != nullptr)
        m_song->playMusic(start);
}

void ApplicationController::pause()
{
    play(false);
}

void ApplicationController::rewind()
{
    if (m_song != nullptr)
        m_song->rewind();
}

void ApplicationController::playFromStartBar()
{
    if (m_song != nullptr)
        m_song->playFromStartBar();
}

void ApplicationController::seekTick(qint64 tick)
{
    tick = boundedSongTick(tick);
    if (m_song != nullptr)
        m_song->seekToTick(tick);
    m_scrollbarVisibleOriginTick = tick;
}

void ApplicationController::pcKeyPress(int key, bool down)
{
    if (m_song != nullptr)
        m_song->pcKeyPress(key, down);
}

float ApplicationController::speed() const
{
    return m_song == nullptr ? 1.0f : m_song->getSpeed();
}

void ApplicationController::setSpeed(float speed)
{
    if (m_song != nullptr)
        m_song->setSpeed(speed);
}

int ApplicationController::transpose() const
{
    return m_song == nullptr ? 0 : m_song->getTranspose();
}

void ApplicationController::setTranspose(int semitones)
{
    if (m_song != nullptr)
        m_song->transpose(semitones);
}

void ApplicationController::setActiveHand(whichPart_t hand)
{
    if (m_settings != nullptr)
        m_settings->saveActiveHandSettings();
    if (m_song != nullptr)
        m_song->setActiveHand(hand);
    if (m_settings != nullptr)
        m_settings->loadActiveHandSettings();
}

whichPart_t ApplicationController::activeHand() const
{
    return CNote::getActiveHand();
}

void ApplicationController::setActiveChannel(int channel)
{
    if (m_song != nullptr)
        m_song->setActiveChannel(channel);
}

int ApplicationController::activeChannel() const
{
    return m_song == nullptr ? 0 : m_song->getActiveChannel();
}

void ApplicationController::setPlayMode(playMode_t mode)
{
    if (m_song != nullptr)
        m_song->setPlayMode(mode);
}

playMode_t ApplicationController::playMode() const
{
    return m_song == nullptr ? PB_PLAY_MODE_listen : m_song->getPlayMode();
}

void ApplicationController::setBoostVolume(int value)
{
    if (m_song != nullptr)
        m_song->boostVolume(value);
}

void ApplicationController::setPianoVolume(int value)
{
    if (m_song != nullptr)
        m_song->pianoVolume(value);
}

void ApplicationController::setMutePianistPart(bool muted)
{
    if (m_song != nullptr)
        m_song->mutePianistPart(muted);
}

void ApplicationController::setTimingMarkers(bool enabled)
{
    if (m_song != nullptr)
        m_song->setTimingMarkers(enabled);
}

bool ApplicationController::timingMarkers() const
{
    return m_song != nullptr && m_song->timingMarkers();
}

void ApplicationController::setStopPointMode(stopPointMode_t mode)
{
    if (m_song != nullptr)
        m_song->setStopPointMode(mode);
}

stopPointMode_t ApplicationController::stopPointMode() const
{
    return m_song == nullptr ? PB_STOP_POINT_MODE_automatic : m_song->stopPointMode();
}

void ApplicationController::setRhythmTappingMode(rhythmTapping_t mode)
{
    if (m_song != nullptr)
        m_song->setRhythmTappingMode(mode);
}

rhythmTapping_t ApplicationController::rhythmTappingMode() const
{
    return m_song == nullptr ? PB_RHYTHM_TAP_drumsOnly : m_song->rhythmTappingMode();
}

double ApplicationController::currentBarPosition() const
{
    return m_song == nullptr ? 0.0 : m_song->getCurrentBarPos();
}

double ApplicationController::playFromBarPosition() const
{
    return m_song == nullptr ? 0.0 : m_song->getPlayFromBar();
}

double ApplicationController::loopingBars() const
{
    return m_song == nullptr ? 0.0 : m_song->getLoopingBars();
}

double ApplicationController::playUptoBarPosition() const
{
    return m_song == nullptr ? 0.0 : m_song->getPlayUptoBar();
}

void ApplicationController::setPlayFromBarPosition(double bar)
{
    if (m_song != nullptr)
        m_song->setPlayFromTick(tickAtBarPosition(bar));
}

void ApplicationController::setLoopingBars(double bars)
{
    if (m_song != nullptr)
        m_song->setLoopingBars(bars);
}

QStringList ApplicationController::midiPortList(CMidiDeviceBase::midiType_t type) const
{
    return m_song == nullptr ? QStringList() : m_song->getMidiPortList(type);
}

void ApplicationController::openMidiPort(CMidiDeviceBase::midiType_t type, const QString& name)
{
    if (m_song != nullptr)
        m_song->openMidiPort(type, name);
}

int ApplicationController::latencyFix() const
{
    return m_song == nullptr ? 0 : m_song->getLatencyFix();
}

void ApplicationController::setLatencyFix(int msec)
{
    if (m_song != nullptr)
        m_song->setLatencyFix(msec);
}

void ApplicationController::setPianoSoundPatches(int rightSound, int wrongSound, bool update)
{
    if (m_song != nullptr)
        m_song->setPianoSoundPatches(rightSound, wrongSound, update);
}

void ApplicationController::testWrongNoteSound(bool enabled)
{
    if (m_song != nullptr)
        m_song->testWrongNoteSound(enabled);
}

void ApplicationController::reconnectMidi()
{
    if (m_song != nullptr)
        m_song->reconnectMidi();
}

void ApplicationController::flushMidiInput()
{
    if (m_song != nullptr)
        m_song->flushMidiInput();
}

CTrackList* ApplicationController::trackList() const
{
    return m_song == nullptr ? nullptr : m_song->getTrackList();
}

void ApplicationController::initializeTrackList(CTrackList *trackList)
{
    if (trackList != nullptr && m_song != nullptr && m_settings != nullptr)
        trackList->init(m_song, m_settings);
}

void ApplicationController::selectTrackRow(int row)
{
    CTrackList *tracks = trackList();
    if (tracks != nullptr)
        tracks->currentRowChanged(row);
}

void ApplicationController::setTrackHands(int leftRow, int rightRow)
{
    CTrackList *tracks = trackList();
    if (tracks != nullptr)
        tracks->setActiveHandsIndex(leftRow, rightRow);
}

void ApplicationController::invalidateActiveScoreCache()
{
    if (m_song != nullptr)
        m_song->invalidateActiveScoreCache();
}

void ApplicationController::invalidateScoreRendererCaches()
{
    if (m_song != nullptr)
        m_song->invalidateScoreRendererCaches();
}

void ApplicationController::rebuildScoreData()
{
    if (m_song != nullptr)
        m_song->rebuildScoreData();
}

void ApplicationController::forceScoreRedraw()
{
    if (m_song != nullptr)
        m_song->forceScoreRedraw();
}

void ApplicationController::regenerateChordTimeline()
{
    if (m_song != nullptr)
        m_song->regenerateChordQueue();
}

ScoreScrollState ApplicationController::scrollbarState() const
{
    ScoreScrollState state;
    if (m_song == nullptr)
        return state;
    state.durationTicks = m_song->songData().durationTicks;
    state.currentTick = m_song->transportState().currentTick;
    state.visibleOriginTick = m_scrollbarFollowPlayback
            ? state.currentTick
            : m_scrollbarVisibleOriginTick;
    state.followPlayback = m_scrollbarFollowPlayback;
    state.dragging = m_scrollbarDragging;
    return state;
}

void ApplicationController::beginScrollbarDrag()
{
    m_scrollbarDragging = true;
    m_scrollbarFollowPlayback = false;
    m_scrollbarVisibleOriginTick = boundedSongTick(scrollbarState().currentTick);
}

void ApplicationController::updateScrollbarDrag(qint64 tick)
{
    m_scrollbarVisibleOriginTick = boundedSongTick(tick);
    if (m_song != nullptr)
        m_song->previewScoreTick(m_scrollbarVisibleOriginTick);
}

void ApplicationController::finishScrollbarDrag(qint64 tick)
{
    m_scrollbarVisibleOriginTick = boundedSongTick(tick);
    seekTick(m_scrollbarVisibleOriginTick);
    m_scrollbarDragging = false;
    m_scrollbarFollowPlayback = true;
}

void ApplicationController::clickScrollbarSeek(qint64 tick)
{
    beginScrollbarDrag();
    finishScrollbarDrag(tick);
}

void ApplicationController::setScrollbarFollowPlayback(bool follow)
{
    m_scrollbarFollowPlayback = follow;
    if (follow)
        m_scrollbarVisibleOriginTick = scrollbarState().currentTick;
}

qint64 ApplicationController::boundedSongTick(qint64 tick) const
{
    if (m_song == nullptr)
        return 0;
    const qint64 duration = m_song->songData().durationTicks;
    if (tick < 0)
        return 0;
    if (tick > duration)
        return duration;
    return tick;
}

qint64 ApplicationController::tickAtBarPosition(double bar) const
{
    if (m_song == nullptr)
        return 0;
    return ::tickAtBarPosition(m_song->barMap(), bar);
}
