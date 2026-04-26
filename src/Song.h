/*********************************************************************************/
/*!
@file           Song.h

@brief          xxx.

@author         L. J. Barman

    Copyright (c)   2008-2013, L. J. Barman, all rights reserved

    This file is part of the PianoBooster application

    PianoBooster is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PianoBooster is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PianoBooster.  If not, see <http://www.gnu.org/licenses/>.

*/
/*********************************************************************************/

#ifndef __SONG_H__
#define __SONG_H__

#include <QString>
#include <QStringList>

#include "Notation.h"
#include "Conductor.h"
#include "TrackList.h"
#include "SongData.h"
#include "BarMap.h"
#include "MidiState.h"

#define PC_KEY_LOWEST_NOTE    58
#define PC_KEY_HIGHEST_NOTE    75

class CSong
{
public:
    CSong()
        : m_scoreWin(nullptr),
          m_settings(nullptr)
    {
        CStavePos::setKeySignature( NOT_USED, 0 );
        m_songDataReadIndex = 0;
        m_songDataReadTick = 0;
        m_playFromBar = 0.0;
        m_loopingBars = 0.0;

        reset();
    }

    void reset()
    {
        m_reachedMidiEof = false;
    }

    void init2(CScore * scoreWin, CSettings* settings);
    eventBits_t task(qint64 ticks);
    bool pcKeyPress(int key, bool down);
    void loadSong(const QString &filename);
    void rebuildScoreData();
    void regenerateChordQueue();
    void playMusic(bool start);
    bool playingMusic() const { return m_conductor.playingMusic(); }
    const TransportState& transportState() const { return m_conductor.transportState(); }

    void rewind();
    void playFromStartBar();
    void seekToTick(qint64 tick);
    void previewScoreTick(qint64 tick);
    void setPlayFromTick(qint64 tick);

    void setActiveHand(whichPart_t hand);
    whichPart_t getActiveHand(){return CNote::getActiveHand();}

    void setActiveChannel(int part);
    int getActiveChannel() const { return m_conductor.getActiveChannel(); }
    void setPlayMode(playMode_t mode);
    playMode_t getPlayMode() const { return m_conductor.getPlayMode(); }
    float getSpeed() const { return m_conductor.getSpeed(); }
    void setSpeed(float speed) { m_conductor.setSpeed(speed); }
    int getTranspose() const { return m_conductor.getTranspose(); }
    void transpose(int semitones) { m_conductor.transpose(semitones); }
    void setSkill(int skill) { m_conductor.setSkill(skill); }
    void setPlayFromBar(double bar);
    void setLoopingBars(double bars);
    double getLoopingBars() const { return m_loopingBars; }
    double getPlayFromBar() const { return m_playFromBar; }
    double getPlayUptoBar() const { return m_playFromBar + m_loopingBars; }
    CTrackList* getTrackList() {return &m_trackList;}
    CRating* getRating() { return m_conductor.getRating(); }
    CChord getWantedChord() { return m_conductor.getWantedChord(); }
    void getTimeSig(int *top, int *bottom) { m_conductor.getTimeSig(top, bottom); }
    void setPianistChannels(int goodChan, int badChan) { m_conductor.setPianistChannels(goodChan, badChan); }
    bool hasPianistKeyboardChannel(int channel) { return m_conductor.hasPianistKeyboardChannel(channel); }
    void mapTrack2Channel(int track, int channel) { m_conductor.mapTrack2Channel(track, channel); }
    void boostVolume(int value) { m_conductor.boostVolume(value); }
    void pianoVolume(int value) { m_conductor.pianoVolume(value); }
    void mutePianistPart(bool muted) { m_conductor.mutePianistPart(muted); }
    void setTimingMarkers(bool enabled) { m_conductor.setTimingMarkers(enabled); }
    bool timingMarkers() const { return m_conductor.timingMarkers(); }
    void setStopPointMode(stopPointMode_t mode) { m_conductor.setStopPointMode(mode); }
    stopPointMode_t stopPointMode() const { return m_conductor.stopPointMode(); }
    void setRhythmTappingMode(rhythmTapping_t mode) { m_conductor.setRhythmTappingMode(mode); }
    rhythmTapping_t rhythmTappingMode() const { return m_conductor.rhythmTappingMode(); }
    QStringList getMidiPortList(CMidiDeviceBase::midiType_t type) { return m_conductor.getMidiPortList(type); }
    bool openMidiPort(CMidiDeviceBase::midiType_t type, const QString &name) { return m_conductor.openMidiPort(type, name); }
    bool validMidiOutput() { return m_conductor.validMidiOutput(); }
    void flushMidiInput() { m_conductor.flushMidiInput(); }
    void playMidiEvent(const CMidiEvent& event) { m_conductor.playMidiEvent(event); }
    int getLatencyFix() const { return m_conductor.getLatencyFix(); }
    void setLatencyFix(int msec) { m_conductor.setLatencyFix(msec); }
    void setPianoSoundPatches(int rightSound, int wrongSound, bool update = false)
    {
        m_conductor.setPianoSoundPatches(rightSound, wrongSound, update);
    }
    void testWrongNoteSound(bool enabled) { m_conductor.testWrongNoteSound(enabled); }
    void reconnectMidi() { m_conductor.reconnectMidi(); }
    void forceScoreRedraw() { m_conductor.forceScoreRedraw(); }
    void invalidateActiveScoreCache();
    void invalidateScoreRendererCaches();

    const QString &getSongTitle() {return m_songTitle;}
    const SongData& songData() const { return m_songData; }
    const BarMap& barMap() const { return m_barMap; }
    int getBarNumber() const;
    double getCurrentBarPos() const;

private:
    void midiFileInfo();
    void resetSongData(const QString &filename);
    void appendSongDataEvent(CMidiEvent event, int streamIndex);
    void buildSongDataMaps();
    void buildSongDataNotes();
    void setSongDataReadPosition(qint64 tick);
    CMidiEvent readSongDataEvent();
    void directSeekToPlayFromBar();
    void restoreMidiStateAtTick(qint64 tick);
    void restoreBankSelectState(const MidiChannelState& state);
    void restoreProgramState(const MidiChannelState& state);
    void restoreControllerState(const MidiChannelState& state);
    void updateTransportLoop();
    ChordAnnotationOptions chordAnnotationOptions() const;

    CScore *m_scoreWin;
    CSettings *m_settings;
    CConductor m_conductor;
    CMidiFile m_midiFile;
    SongData m_songData;
    BarMap m_barMap;
    int m_songDataReadIndex;
    qint64 m_songDataReadTick;
    double m_playFromBar;
    double m_loopingBars;
    bool m_reachedMidiEof;
    CChord m_fakeChord;  // the chord played with the tab key
    CTrackList m_trackList;
    QString m_songTitle;
};

#endif  // __SONG_H__
