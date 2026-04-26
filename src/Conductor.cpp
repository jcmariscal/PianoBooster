/*********************************************************************************/
/*!
@file           Conductor.cpp

@brief          xxxx.

@author         L. J. Barman

    Copyright (c)   2008-2020, L. J. Barman, all rights reserved

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

#define OPTION_DEBUG_CONDUCTOR     0
#if OPTION_DEBUG_CONDUCTOR
#define ppDEBUG_CONDUCTOR(args)     ppLogDebug args
#else
#define ppDEBUG_CONDUCTOR(args)
#endif

#include "Conductor.h"
#include "Score.h"
#include "Piano.h"
#include "Cfg.h"

#include <QObject>

CConductor::CConductor()
    : m_savedNoteQueue(200),
      m_savedNoteOffQueue(200)
{
    m_scoreWin = nullptr;
    m_settings = nullptr;
    m_piano = nullptr;

    m_transpose = 0;
    m_playMode = PB_PLAY_MODE_listen;
    m_latencyFix = 0;
    m_leadLagAdjust = 0;
    setSpeed(1.0);
    setLatencyFix(0);
    m_boostVolume = 0;
    m_pianoVolume = 0;
    m_activeChannel = 0;
    m_skill = 0;
    m_chordTimelineIndex = 0;
    m_currentSongTickRemainder = 0;
    m_silenceTimeOut = 0;
    m_realTimeEventBits = 0;
    m_mutePianistPart = false;
    setPianistChannels(1-1,2-1);
    m_timingMarkersFlag = false;
    m_stopPointMode = PB_STOP_POINT_MODE_automatic;
    m_rhythmTappingMode = PB_RHYTHM_TAP_drumsOnly;
    m_cfg_rhythmTapRightHandDrumSound = 40;
    m_cfg_rhythmTapLeftHandDrumSound = 41;

    setPianoSoundPatches(1-1, 7-1); // 6-1
    m_tempo.setSavedWantedChord(&m_savedWantedChord);

    reset();
    rewind();
    testWrongNoteSound(false);
}

void CConductor::reset()
{
    int i;
    for ( i = 0; i < MAX_MIDI_TRACKS; i++)
    {
        mapTrack2Channel(i,   i);
        if (i >= MAX_MIDI_CHANNELS)
            mapTrack2Channel(i, -1);
    }
    clearMutedSplitNotes();
    clearPracticeState(&m_practiceState);
}

QStringList CConductor::getMidiPortList(CMidiDeviceBase::midiType_t type)
{
    return m_midiDevice.getMidiPortList(type);
}

bool CConductor::openMidiPort(CMidiDeviceBase::midiType_t type, const QString &portName)
{
    return m_midiDevice.openMidiPort(type, portName);
}

bool CConductor::validMidiOutput()
{
    return m_midiDevice.validMidiOutput();
}

void CConductor::flushMidiInput()
{
    m_midiDevice.flushMidiInput();
}

void CConductor::playMidiEvent(const CMidiEvent &event)
{
    m_midiDevice.playMidiEvent(event);
}

int CConductor::checkMidiInput()
{
    return m_midiDevice.checkMidiInput();
}

CMidiEvent CConductor::readMidiInput()
{
    return m_midiDevice.readMidiInput();
}

eventBits_t CConductor::takeEventBits()
{
    eventBits_t bits = m_realTimeEventBits;
    m_realTimeEventBits = 0;
    return bits;
}

void CConductor::setChordTimeline(const ChordTimeline& timeline)
{
    m_chordTimeline = timeline;
    setChordTimelinePosition(currentSongTick());
}

void CConductor::setChordTimelinePosition(qint64 tick)
{
    m_chordTimelineIndex = chordTimelineIndexAt(m_chordTimeline, tick);
    resetWantedChord();
}

void CConductor::setPlaybackEvents(const QVector<MidiEventRecord>* events)
{
    setPlaybackCursorEvents(m_playbackCursor, events);
    m_nextMidiEvent.clear();
}

void CConductor::setPlaybackReadPosition(qint64 tick)
{
    seekPlaybackCursor(m_playbackCursor, tick);
    m_nextMidiEvent.clear();
}

bool CConductor::readNextPlaybackEvent(CMidiEvent& event)
{
    return readPlaybackCursorEvent(m_playbackCursor, event);
}

void CConductor::channelSoundOff(int channel)
{
    if (channel < 0 || channel >= MAX_MIDI_CHANNELS)
    {
        return;
    }

    CMidiEvent midi;
    midi.controlChangeEvent(0, channel, MIDI_ALL_NOTES_OFF, 0);
    playMidiEvent(midi);
    // remove the sustain pedal as well
    midi.controlChangeEvent(0, channel, MIDI_SUSTAIN, 0);
    playMidiEvent(midi);
}

void CConductor::allSoundOff()
{
    int channel;

    for ( channel = 0; channel < MAX_MIDI_CHANNELS; channel++)
    {
        if (channel != m_pianistGoodChan)
            channelSoundOff(channel);
    }
    m_savedNoteQueue.clear();
    m_savedNoteOffQueue.clear();
    clearMutedSplitNotes();
}

void CConductor::clearMutedSplitNotes()
{
    for (int chan = 0; chan < MAX_MIDI_CHANNELS; chan++)
        for (int note = 0; note < MAX_MIDI_NOTES; note++)
            m_mutedSplitNotes[chan][note] = 0;
}

bool CConductor::usesSplitListenVolume(int chan) const
{
    return m_playMode == PB_PLAY_MODE_listen &&
            chan >= 0 && chan < MAX_MIDI_CHANNELS &&
            CNote::splitHandsForChannel(m_activeChannel) &&
            CNote::splitHandsForChannel(chan);
}

void CConductor::resetAllChannels()
{
    int channel;

    CMidiEvent midi;
    for ( channel = 0; channel < MAX_MIDI_CHANNELS; channel++)
    {
        midi.controlChangeEvent(0, channel, MIDI_RESET_ALL_CONTROLLERS, 0);
        playMidiEvent(midi);
    }
}

/* calculate the new solo_volume */
int CConductor::calcBoostVolume(int channel, int volume)
{
    int returnVolume;
    bool activePart;

    if (volume == -1)
        volume = m_savedMainVolume[channel];
    else
        m_savedMainVolume[channel] = volume;

    returnVolume = volume;
    if (usesSplitListenVolume(channel))
    {
        if (m_pianoVolume>0)
            returnVolume = (returnVolume * (100 - m_pianoVolume)) / 100;
        if (returnVolume > 127)
            returnVolume = 127;
        return returnVolume;
    }

    activePart = false;
    if (CNote::hasPianoPart(m_activeChannel))
    {
        if (m_playMode == PB_PLAY_MODE_listen) // only boost one hand in listen mode
        {
            if (channel == CNote::leftHandChan() && CNote::getActiveHand() != PB_PART_right)
                activePart = true;
            if (channel == CNote::rightHandChan() && CNote::getActiveHand() != PB_PART_left)
                activePart = true;
        }
        else // otherwise always boost both hands
        {
            if ( CNote::hasPianoPart(channel))
                activePart = true;
        }
    }
    else
    {
        if (channel == m_activeChannel)
            activePart= true;
    }
    if (channel == m_activeChannel)
        activePart= true;

    //if (channel == 5)  activePart= true; // for debugging

    if (activePart)
    {
        if (returnVolume == 0 )
            ;               /* Don't adjust the volume if zero */
        else if (m_boostVolume < 0 )
            returnVolume = (returnVolume * (m_boostVolume + 100)) / 100;
        else
            returnVolume += m_boostVolume;
    }
    else
    {
        if (m_boostVolume > 0)
            returnVolume = (returnVolume * (100 - m_boostVolume)) / 100;
    }
        // The piano volume can reduce the volume of the music
    if (m_pianoVolume>0)
        returnVolume = (returnVolume * (100 - m_pianoVolume)) / 100;;

    if (returnVolume > 127)
        returnVolume = 127;
    return returnVolume;
}

int CConductor::calcSplitListenVelocity(const CMidiEvent& event)
{
    int velocity = event.velocity();
    if (!usesSplitListenVolume(event.channel()) || velocity <= 0)
        return velocity;

    const whichPart_t eventHand = CNote::findHand(event, m_activeChannel, PB_PART_both);
    if (eventHand == PB_PART_none)
        return velocity;

    const whichPart_t activeHand = CNote::getActiveHand();
    const bool activePart = activeHand == PB_PART_both || activeHand == eventHand;
    if (activePart)
    {
        if (m_boostVolume < 0)
            velocity = (velocity * (m_boostVolume + 100)) / 100;
        else
            velocity += m_boostVolume;
    }
    else if (m_boostVolume > 0)
    {
        velocity = (velocity * (100 - m_boostVolume)) / 100;
    }

    if (velocity < 0)
        velocity = 0;
    if (velocity > 127)
        velocity = 127;
    return velocity;
}

bool CConductor::applySplitListenVolume(CMidiEvent& event)
{
    if (!usesSplitListenVolume(event.channel()))
        return false;

    const int note = event.note();
    const int channel = event.channel();
    if (note < 0 || note >= MAX_MIDI_NOTES)
        return false;

    if (event.type() == MIDI_NOTE_OFF)
    {
        if (m_mutedSplitNotes[channel][note] > 0)
        {
            m_mutedSplitNotes[channel][note]--;
            return true;
        }
        return false;
    }

    if (event.type() != MIDI_NOTE_ON)
        return false;

    const int velocity = calcSplitListenVelocity(event);
    if (velocity <= 0)
    {
        m_mutedSplitNotes[channel][note]++;
        return true;
    }

    event.setVelocity(velocity);
    return false;
}

/* send boost volume by adjusting all channels */
void CConductor::outputBoostVolume()
{
    int chan;

    for ( chan =0; chan <MAX_MIDI_CHANNELS; chan++ )
    {
        if (hasPianistKeyboardChannel(chan))
            continue;
        CMidiEvent midi;
        midi.controlChangeEvent(0, chan, MIDI_MAIN_VOLUME, calcBoostVolume(chan,-1));
        playTrackEvent(midi);
    }
    outputPianoVolume();
}

void CConductor::transpose(int transpose)
{
    if (m_transpose != transpose)
    {
        allSoundOff();
        m_transpose = transpose;
        if (m_transpose >  12)
            m_transpose = 12;
        if (m_transpose < -12)
            m_transpose = -12;
        m_scoreWin->transpose(m_transpose);
    }
}

void CConductor::mutePianistPart(bool state)
{
    m_mutePianistPart = state;
}

void CConductor::setActiveHand(whichPart_t hand)
{
    if (CNote::getActiveHand() == hand)
        return;
    CNote::setActiveHand(hand);
    outputBoostVolume();
    resetWantedChord();

    findSplitPoint();
    forceScoreRedraw();
}

void CConductor::setPlayMode(playMode_t mode)
{
    m_playMode = mode;
    m_rating.setPlayAlongMode(m_playMode == PB_PLAY_MODE_playAlong);
    if ( m_playMode == PB_PLAY_MODE_listen )
        resetWantedChord();
    outputBoostVolume();
    clearPracticeState(&m_practiceState);
    m_piano->setRhythmTapping(m_playMode == PB_PLAY_MODE_rhythmTapping);
}

void CConductor::setActiveChannel(int channel)
{
    m_activeChannel = channel;
    outputBoostVolume();
    resetWantedChord();
}

void CConductor::outputPianoVolume()
{
    CMidiEvent event;
    int volume = 127;
    // if piano volume is between -100 and 0 reduce the volume accordingly
    if (m_pianoVolume < 0)
        volume = (volume * (100 + m_pianoVolume)) / 100;

    event.controlChangeEvent(0, m_pianistGoodChan, MIDI_MAIN_VOLUME, volume);
    playTrackEvent(event); // Play the midi note or event
    event.controlChangeEvent(0, m_pianistBadChan, MIDI_MAIN_VOLUME, volume);
    playTrackEvent(event); // Play the midi note or event
}

void CConductor::updatePianoSounds()
{
    CMidiEvent event;

    if (m_cfg_rightNoteSound>=0) // ignore if set to -1 (tr("None"))
    {
        event.programChangeEvent(0, m_pianistGoodChan, m_cfg_rightNoteSound);
        playTrackEvent( event );
    }
    if (m_cfg_wrongNoteSound>=0)
    {
        event.programChangeEvent(0, m_pianistBadChan, m_cfg_wrongNoteSound);
        playTrackEvent( event );
    }
}

void CConductor::testWrongNoteSound(bool enable)
{
    m_testWrongNoteSound = enable;
    updatePianoSounds();
}

void CConductor::reconnectMidi()
{
    if (!validMidiOutput()) {
        QString midiInputName = m_settings->value("Midi/Input").toString();
        if (midiInputName.startsWith(QObject::tr("None"))) {
            CChord::setPianoRange(PC_KEY_LOWEST_NOTE, PC_KEY_HIGHEST_NOTE);
        } else {
            CChord::setPianoRange(m_settings->value("Keyboard/LowestNote", 0).toInt(),
                             m_settings->value("Keyboard/HighestNote", 127).toInt());
        }
        openMidiPort(CMidiDevice::MIDI_INPUT, midiInputName);
        openMidiPort(CMidiDevice::MIDI_OUTPUT,m_settings->value("Midi/Output").toString());
    }
    m_settings->updateWarningMessages();
}

void CConductor::playMusic(bool start)
{
    reconnectMidi();
    setTransportPlaying(m_transport, start);
    allSoundOff();
    if (start)
    {
        resetAllChannels();

        testWrongNoteSound(false);
        outputBoostVolume();
        if (seekingBarNumber())
            resetWantedChord();

        /*
        const unsigned char gsModeEnterData[] =  {0xf0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7f, 0x00, 0x41, 0xf7};

        for (auto &d : gsModeEnterData)
        {
            event.collateRawByte(0, d);
            playTrackEvent(event);
        }
        event.outputCollatedRawBytes(0);
        playTrackEvent(event);
        */
    }
}

// This will allow us to map midi tracks onto midi channels
// tacks will eventually allow for more than the 16 midi channels (eg with two mid devices)
// At the moment tracks are only used to remap midi events from channel 0 when using the keyboard lights.
void CConductor::playTrackEvent(CMidiEvent event)
{
    int track = event.channel();
    int chan = track2Channel(track);
    if (chan == -1)
        return;
    event.setChannel(chan);
    playMidiEvent(event);
}

void CConductor::playTransposeEvent(CMidiEvent event)
{
    if (m_transpose != 0 && event.channel() != MIDI_DRUM_CHANNEL &&
                (event.type() == MIDI_NOTE_ON || event.type() == MIDI_NOTE_OFF) )
        event.transpose(m_transpose);

    if ((event.type() == MIDI_NOTE_ON || event.type() == MIDI_NOTE_OFF) && applySplitListenVolume(event))
        return;

     if (event.type() == MIDI_NOTE_ON && CChord::isPianistNote(event, m_transpose, getActiveChannel()) && shouldMutePianistPart()) {
        return; // mute the note by not playing it
     }

    // boost any volume events
    if (event.type() == MIDI_CONTROL_CHANGE && event.data1() == MIDI_MAIN_VOLUME)
        event.setDatat2(calcBoostVolume(event.channel(), event.data2() ));

    // Don't output note on if we are seeking to bar
    if (!seekingBarNumber())
        playTrackEvent(event); // Play the midi note or event
    else
    {
        if (event.type() == MIDI_PROGRAM_CHANGE || event.type() == MIDI_CONTROL_CHANGE)
            playTrackEvent(event); // Play the midi note or event
    }
}

void CConductor::playSeekRestoreEvent(CMidiEvent event)
{
    if (event.type() != MIDI_PROGRAM_CHANGE && event.type() != MIDI_CONTROL_CHANGE)
        return;
    event.setDeltaTime(0);
    playTransposeEvent(event);
}

void CConductor::outputSavedNotes()
{
    // The saved notes off are note needed any more
    // (as the are also in the savedNoteQueue
    m_savedNoteOffQueue.clear();

    // output any the saved up notes
    while (m_savedNoteQueue.length() > 0)
        playTransposeEvent(m_savedNoteQueue.pop());
}

void CConductor::resetWantedChord()
{
    m_wantedChord.clear();
    ppDEBUG_CONDUCTOR(("resetWantedChord m_chordDeltaTime %d m_playingDeltaTime %d", m_chordDeltaTime, m_playingDeltaTime ));

    m_followPlayingTimeOut = false;
    m_chordDeltaTime = m_playingDeltaTime;
    m_pianistTiming = m_chordDeltaTime;
    m_pianistSplitPoint = MIDDLE_C;

    outputSavedNotes();
    m_followState = PB_FOLLOW_searching;
}

// switch modes if we are playing well enough (i.e. don't slow down if we are playing late)
void CConductor::setFollowSkillAdvanced(bool enable)
{
    if (m_settings==nullptr || m_scoreWin == nullptr)
        return;

    m_settings-> setAdvancedMode(enable);

    if (getLatencyFix() > 0)
    {
        // behave differently if we are using the latency fix
        m_cfg_earlyNotesPoint = m_cfg_imminentNotesOffPoint; //disable the early notes
    }

    if (m_stopPointMode == PB_STOP_POINT_MODE_onTheBeat)
        enable = false;
    if (m_stopPointMode == PB_STOP_POINT_MODE_afterTheBeat)
        enable = true;
    if (m_playMode == PB_PLAY_MODE_rhythmTapping)
        enable = true;

    m_followSkillAdvanced = enable;
    m_stopPoint = (enable) ? m_cfg_stopPointAdvanced: m_cfg_stopPointBeginner ;
}

void CConductor::findSplitPoint()
{
    // find the split point
    int lowestTreble = MIDDLE_C + 37;
    int highestBase =  MIDDLE_C - 37;
    CNote note;

    // Find where to put the split point
    for(int i = 0; i < m_wantedChord.length(); i++)
    {
        note = m_wantedChord.getNote(i);
        if (note.part() == PB_PART_right && note.pitch() < lowestTreble)
            lowestTreble = note.pitch();
        if (note.part() == PB_PART_left && note.pitch() > highestBase)
            highestBase = note.pitch();
    }

    //put the split point in the middle
    m_pianistSplitPoint = ((lowestTreble + highestBase) /2 ) + m_transpose;
}

void CConductor::turnOnKeyboardLights(bool on)
{
    int note;
    int i;
    CMidiEvent event;

    // exit if not enable
    if (Cfg::keyboardLightsChan == -1)
        return;

    // exit if keyboard light are already on
    if (m_KeyboardLightsOn && on)
        return;

    m_KeyboardLightsOn = on;
    for(i = 0; i < m_wantedChord.length(); i++)
    {
        note = m_wantedChord.getNote(i).pitch();
        if (on == true)
            event.noteOnEvent(0, Cfg::keyboardLightsChan, note, 1);
        else
            event.noteOffEvent(0, Cfg::keyboardLightsChan, note, 1);
       playMidiEvent( event ); // don't use the track  settings
    }
}

void CConductor::fetchNextChord()
{
    m_followState = PB_FOLLOW_searching;
    m_followPlayingTimeOut = false;

    outputSavedNotes();
    turnOnKeyboardLights(false);

    do // Remove notes or chords that are out of our range
    {
        if (m_chordTimelineIndex >= m_chordTimeline.chords.size())
        {
            m_wantedChord.clear();
            m_pianistSplitPoint = MIDDLE_C;
            return;
        }

        const TimelineChord entry = m_chordTimeline.chords[m_chordTimelineIndex++];
        m_wantedChord = entry.chord;
        m_savedWantedChord = m_wantedChord;
        m_chordDeltaTime = (currentSongTick() - entry.tick) * SPEED_ADJUST_FACTOR;
        m_pianistTiming = m_chordDeltaTime;
    }
    while (m_wantedChord.trimOutOfRangeNotes(m_transpose)==0);

    // now find the split point
    findSplitPoint();
}

bool CConductor::validatePianistNote( const CMidiEvent & inputNote)
{
    return practiceNoteMatchesChord(m_wantedChord, inputNote, m_transpose,
                                    m_chordDeltaTime, m_cfg_playZoneEarly);
}

void CConductor::playWantedChord (CChord chord, CMidiEvent inputNote)
{
    int pitch;
    for(int i = 0; i < chord.length(); i++)
    {
        pitch = chord.getNote(i).pitch();
        inputNote.setNote(pitch);
        playTrackEvent(inputNote);
    }
}

bool CConductor::validatePianistChord()
{
    return practiceChordComplete(m_goodPlayedNotes.length(), m_wantedChord.length(),
                                 m_piano->pianistBadNotesDown(), m_skill);
}

/**
 * Add in the extra notes in rhythm practice
 */
void CConductor::expandPianistInput(CMidiEvent inputNote)
{
    if (m_playMode == PB_PLAY_MODE_rhythmTapping)
    {
        CChord chord;
        int i;
        CMidiEvent newNote = inputNote;
        if (inputNote.type() == MIDI_NOTE_ON || inputNote.type() == MIDI_NOTE_OFF)
        {
            chord = m_wantedChord;
            CChord chordForOneHand;
            int notesFound = 0;

            if (inputNote.type() == MIDI_NOTE_OFF)
            {
                chord = takeRhythmChord(&m_practiceState, inputNote.note());
                for(i = 0; i < chord.length(); i++)
                {
                    inputNote.setNote( chord.getNote(i).pitch());
                    pianistInput(inputNote);
                }

                // We have already played and removed this chord
                if (chord.length() > 0)
                    return;
            }

            whichPart_t targetPart = (inputNote.note() >= MIDDLE_C) ? PB_PART_right : PB_PART_left;
            for(i = 0; i < chord.length(); i++)
            {
                if (chord.getNote(i).part() == targetPart  && playingMusic())
                {
                    newNote.setNote(chord.getNote(i).pitch());
                    if ( notesFound >= 1 && m_rhythmTappingMode == PB_RHYTHM_TAP_drumsOnly)
                        newNote.setVelocity(-1);
                    else
                        chordForOneHand.addNote(targetPart, chord.getNote(i).pitch());
                    pianistInput(newNote);
                    notesFound++;
                }
            }
            if (notesFound > 0)
                saveRhythmChord(&m_practiceState, inputNote.note(), chordForOneHand);
            else
            {
                inputNote.setChannel(MIDI_DRUM_CHANNEL);
                pianistInput(inputNote);
            }
        }
        else
        {
            pianistInput(inputNote);
        }
    }
    else
    {
        pianistInput(inputNote);
    }
}

void CConductor::pianistInput(CMidiEvent inputNote)
{
    bool goodSound = true;

    // inputNote.transpose(+12); fixme

    if (m_testWrongNoteSound)
        goodSound = false;

    whichPart_t hand;
    hand = (inputNote.note() >= m_pianistSplitPoint)? PB_PART_right : PB_PART_left;

    // for rhythm tapping
    if ( inputNote.channel() == MIDI_DRUM_CHANNEL)
        hand = (inputNote.note() >= MIDDLE_C) ? PB_PART_right : PB_PART_left;

    if (inputNote.type() == MIDI_NOTE_ON)
    {

        if ( validatePianistNote(inputNote) == true)
        {
            m_goodPlayedNotes.addNote(hand, inputNote.note());
            m_piano->addPianistNote(hand, inputNote,true);
            const qint64 pianistTiming = practiceTimingMarker(
                        m_timingMarkersFlag, m_followSkillAdvanced,
                        m_playMode, PB_PLAY_MODE_rhythmTapping, m_pianistTiming);
            m_scoreWin->setNoteFeedback(inputNote.note(),
                        playedNoteFeedback(m_followPlayingTimeOut),
                        m_chordDeltaTime, pianistTiming);

            if (validatePianistChord() == true)
            {
                if (m_chordDeltaTime < 0)
                    m_tempo.removePlayingTicks(-m_chordDeltaTime);

                m_goodPlayedNotes.clear();
                fetchNextChord();
                // count the good notes so that the live percentage looks OK
                applyPracticeRating(totalNotesEvent(m_wantedChord.length()));
                m_rating.calculateAccuracy();
                m_settings->pianistActive();
                if (m_rating.isAccuracyGood() || m_playMode == PB_PLAY_MODE_playAlong)
                    setFollowSkillAdvanced(true); // change the skill level only when they are good enough
                else
                    setFollowSkillAdvanced(false);
                setEventBits( EVENT_BITS_forceRatingRedraw);
            }
        }
        else
        {
            if (playingMusic())
            {
                goodSound = false;

                m_piano->addPianistNote(hand, inputNote, false);
                applyPracticeRating(wrongNotesEvent(1));

                if (m_settings->followThroughErrors() && m_playMode == PB_PLAY_MODE_followYou) // If the setting is checked, errors cause following too
                  {
                    if (m_chordDeltaTime <= -m_cfg_playZoneEarly) // We're hitting bad notes, but earlier than the zone (so ignore them)
                      {
                    m_piano->clear();
                    clearPracticeState(&m_practiceState);
                    m_savedNoteQueue.clear();
                    m_savedNoteOffQueue.clear();
                      }
                    else // Hitting bad notes within the zone (so register them)
                      {
                    // Register the wrong note in the ratings calculation (if not as missed notes, I don't believe it's factored in)
                    markMissingNotes(PracticeFeedbackMissed);
                    applyPracticeRating(lateNotesEvent(
                                            m_wantedChord.length() - m_goodPlayedNotes.length()));
                    setEventBits(EVENT_BITS_forceRatingRedraw);
                    fetchNextChord(); // Skip through the wrong note and continue to the next

                    // Was the next note the one keyed by accident (dyslexia)?  If so, validate it instead and continue as usual.
                    if (m_wantedChord.searchChord(inputNote.note(), m_transpose)) // replaces validatePianistNote ignoring out-of-zone
                      {
                        m_goodPlayedNotes.addNote(hand, inputNote.note());
                        m_piano->addPianistNote(hand, inputNote,true);
                        const qint64 pianistTiming = practiceTimingMarker(
                                    m_timingMarkersFlag, m_followSkillAdvanced,
                                    m_playMode, PB_PLAY_MODE_rhythmTapping, m_pianistTiming);
                        m_scoreWin->setNoteFeedback(inputNote.note(),
                                    playedNoteFeedback(m_followPlayingTimeOut),
                                    m_chordDeltaTime, pianistTiming);

                        if (validatePianistChord() == true)
                          {
                        if (m_chordDeltaTime < 0)
                          m_tempo.removePlayingTicks(-m_chordDeltaTime);

                        m_goodPlayedNotes.clear();
                        fetchNextChord();
                        // count the good notes so that the live percentage looks OK
                        applyPracticeRating(totalNotesEvent(m_wantedChord.length()));
                        m_rating.calculateAccuracy();
                        m_settings->pianistActive();
                        if (m_rating.isAccuracyGood() || m_playMode == PB_PLAY_MODE_playAlong)
                          setFollowSkillAdvanced(true); // change the skill level only when they are good enough
                        else
                          setFollowSkillAdvanced(false);
                        setEventBits( EVENT_BITS_forceRatingRedraw);
                          }
                      }

                    // Clear & ignore any further slips until in range of the next note's zone
                    m_piano->clear();
                    clearPracticeState(&m_practiceState);
                    m_savedNoteQueue.clear();
                    m_savedNoteOffQueue.clear();

                      }
                  }

            }
            else
                m_piano->addPianistNote(hand, inputNote, true);
        }
    }
    else if (inputNote.type() == MIDI_NOTE_OFF)
    {
        if (m_piano->removePianistNote(inputNote.note()) ==  true)
            goodSound = false;
        bool hasNote = m_goodPlayedNotes.removeNote(inputNote.note());

        if (hasNote)
            m_scoreWin->setNoteFeedback(inputNote.note(),
                    releasedNoteFeedback(m_followPlayingTimeOut), m_chordDeltaTime);

        outputSavedNotesOff();
    }

    if ( inputNote.velocity() == -1 )
        return;

    if (goodSound == true || m_cfg_wrongNoteSound < 0)
    {
        if (m_cfg_rightNoteSound >= 0) // don't play anything if the sound is set to -1 (none)
        {
            bool playDrumBeat = false;
            if ( inputNote.channel() != MIDI_DRUM_CHANNEL)
            {
                if (m_rhythmTappingMode != PB_RHYTHM_TAP_drumsOnly || m_playMode != PB_PLAY_MODE_rhythmTapping)
                {
                    inputNote.setChannel(m_pianistGoodChan);
                    playTrackEvent( inputNote );
                }
            }
            else
            {
                playDrumBeat = true;
            }

            if (m_rhythmTappingMode != PB_RHYTHM_TAP_mellodyOnly && m_playMode == PB_PLAY_MODE_rhythmTapping)
                playDrumBeat = true;

            if (playDrumBeat)
            {
                inputNote.setChannel(MIDI_DRUM_CHANNEL);
                ppLogTrace("note %d", inputNote.note());
                inputNote.setNote((hand == PB_PART_right)? m_cfg_rhythmTapRightHandDrumSound : m_cfg_rhythmTapLeftHandDrumSound);
                playTrackEvent( inputNote );
            }
        }
    }
    else
    {
        inputNote.setChannel(m_pianistBadChan);
        if (m_playMode == PB_PLAY_MODE_rhythmTapping)
        {
            inputNote.setChannel(MIDI_DRUM_CHANNEL);
            ppLogTrace("note %d", inputNote.note());
            inputNote.setNote((hand == PB_PART_right)? m_cfg_rhythmTapRightHandDrumSound : m_cfg_rhythmTapLeftHandDrumSound);
        }
        playTrackEvent( inputNote );
    }

    /*
    // use the same channel for the right and wrong note
    int pianoSound = (goodSound == true) ? m_cfg_rightNoteSound : m_cfg_wrongNoteSound;

    if (pianoSound != m_lastSound)
    {
        m_lastSound = pianoSound;

        CMidiEvent midiSound;
        midiSound.programChangeEvent(0,inputNote.channel(),pianoSound);
        playTrackEvent( midiSound );
    }
    */

}

void CConductor::addDeltaTime(qint64 ticks)
{
    m_playingDeltaTime += ticks;
    m_chordDeltaTime += ticks;
}

void CConductor::resetCurrentSongTickRemainder()
{
    m_currentSongTickRemainder = 0;
}

void CConductor::advanceCurrentSongTick(qint64 ticks)
{
    m_currentSongTickRemainder += ticks;
    const qint64 wholeTicks = deltaAdjustL(m_currentSongTickRemainder);
    if (wholeTicks != 0)
    {
        advanceTransportTick(m_transport, wholeTicks);
        m_currentSongTickRemainder -= wholeTicks * SPEED_ADJUST_FACTOR;
    }
    if (m_scoreWin != nullptr)
        m_scoreWin->setCurrentTick(currentSongTick());
}

void CConductor::seekForwardToTick(qint64 tick)
{
    if (tick <= currentSongTick())
        return;

    allSoundOff();
    m_savedNoteQueue.clear();
    m_savedNoteOffQueue.clear();
    m_nextMidiEvent.clear();
    m_goodPlayedNotes.clear();
    clearPracticeState(&m_practiceState);
    m_playingDeltaTime = 0;
    m_chordDeltaTime = 0;
    resetCurrentSongTickRemainder();
    if (m_piano)
        m_piano->clear();

    const qint64 delta = (tick - currentSongTick()) * SPEED_ADJUST_FACTOR + 1;
    m_bar.addDeltaTime(delta);
    m_bar.clearSeeking();
    setEventBits(m_bar.readEventBits());
    advanceCurrentSongTick(delta);
    setChordTimelinePosition(currentSongTick());
}

void CConductor::followPlaying()
{
    if ( m_playMode == PB_PLAY_MODE_listen )
        return;

    if (m_wantedChord.length() == 0)
        fetchNextChord();

    if (m_wantedChord.length() == 0)
        return;

    if (seekingBarNumber())
    {
        if (deltaAdjustL(m_chordDeltaTime) > -m_stopPoint )
            fetchNextChord();
    }
    else if ( m_playMode == PB_PLAY_MODE_followYou ||  m_playMode == PB_PLAY_MODE_rhythmTapping )
    {
        if (deltaAdjustL(m_chordDeltaTime) > -m_cfg_earlyNotesPoint )
            m_followState = PB_FOLLOW_earlyNotes;
        if (deltaAdjustL(m_chordDeltaTime) > -m_stopPoint )
        {
            m_followState = PB_FOLLOW_waiting;
            // Throw away the time past the stop point (by adding a negative ticks)
            addDeltaTime( -m_stopPoint*SPEED_ADJUST_FACTOR - m_chordDeltaTime);
        }
    }
    else // m_playMode == PB_PLAY_MODE_playAlong
    {
        if (m_chordDeltaTime > m_cfg_playZoneLate )
        {
            markMissingNotes(PracticeFeedbackStopped);
            fetchNextChord();
            applyPracticeRating(lateNotesEvent(m_wantedChord.length() - m_goodPlayedNotes.length()));
            setEventBits( EVENT_BITS_forceRatingRedraw);
        }
    }
    if (deltaAdjustL(m_chordDeltaTime) > -m_cfg_earlyNotesPoint )
        turnOnKeyboardLights(true);
}

void CConductor::outputSavedNotesOff()
{
    while (m_savedNoteOffQueue.length() > 0)
        playTransposeEvent(m_savedNoteOffQueue.pop()); // Output the saved note off events
}

// untangle the sound in case there is any notes off just after we have stopped
void CConductor::findImminentNotesOff()
{
    PlaybackCursor cursor = m_playbackCursor;
    CMidiEvent event = m_nextMidiEvent;
    qint64 aheadDelta = 0;

    while (deltaAdjustL(m_playingDeltaTime) + aheadDelta   > m_cfg_imminentNotesOffPoint)
    {
        if (event.type() == MIDI_NOTE_OFF )
            m_savedNoteOffQueue.push(event);
        if (!readPlaybackCursorEvent(cursor, event))
            break;
        aheadDelta -= event.deltaTime();
    }
}

void CConductor::applyPracticeRating(PracticeRatingEvent event)
{
    if (event.count <= 0)
        return;
    if (event.type == PracticeRatingTotalNotes)
        m_rating.totalNotes(event.count);
    else if (event.type == PracticeRatingWrongNotes)
        m_rating.wrongNotes(event.count);
    else if (event.type == PracticeRatingLateNotes)
        m_rating.lateNotes(event.count);
}

void CConductor::markMissingNotes(PracticeFeedbackKind kind)
{
    int i;
    CNote note;
    for (i = 0; i < m_wantedChord.length(); i++)
    {
        note = m_wantedChord.getNote(i);
        if (m_goodPlayedNotes.searchChord(note.pitch(),m_transpose) == false)
            m_scoreWin->setNoteFeedback(note.pitch() + m_transpose, kind, m_chordDeltaTime);
    }
}

void CConductor::realTimeEngine(qint64 mSecTicks)
{
    auto ticks = m_tempo.mSecToTicks(mSecTicks);
    if (!m_followPlayingTimeOut)
        m_pianistTiming += ticks;

    while (checkMidiInput() > 0)
        expandPianistInput(readMidiInput());

    if (getfollowState() == PB_FOLLOW_waiting )
    {
        if (m_silenceTimeOut > 0)
        {
            m_silenceTimeOut -= mSecTicks;
            if (m_silenceTimeOut <= 0)
            {
                allSoundOff();
                m_silenceTimeOut = 0;
            }
        }

        m_tempo.insertPlayingTicks(ticks);

        if (m_pianistTiming > m_cfg_playZoneLate)
        {
            if (m_followPlayingTimeOut == false)
            {
                m_followPlayingTimeOut = true;

                m_tempo.clearPlayingTicks();
                applyPracticeRating(lateNotesEvent(
                                        m_wantedChord.length() - m_goodPlayedNotes.length()));
                setEventBits( EVENT_BITS_forceRatingRedraw);

                markMissingNotes(PracticeFeedbackStopped);
                findImminentNotesOff();
                // Don't keep any saved notes off if there are no notes down
                if (m_piano->pianistAllNotesDown() == 0)
                    outputSavedNotesOff();
                m_silenceTimeOut = Cfg::silenceTimeOut();
            }
        }
        return;
    }

    m_silenceTimeOut = 0;
    if (!playingMusic())
        return;

    m_tempo.adjustTempo(&ticks);

    ticks = m_bar.addDeltaTime(ticks);

    setEventBits( m_bar.readEventBits());
    advanceCurrentSongTick(ticks);
    if (transportLoopReached(m_transport))
        setEventBits(EVENT_BITS_UptoBarReached);
#if OPTION_DEBUG_CONDUCTOR
    if (m_realTimeEventBits | EVENT_BITS_newBarNumber)
    {
        ppDEBUG_CONDUCTOR(("m_savedNoteQueue %d m_playingDeltaTime %d",m_savedNoteQueue.space() , m_playingDeltaTime ));
        ppDEBUG_CONDUCTOR(("getfollowState() %d  %d", getfollowState(), m_leadLagAdjust));
    }
#endif

    addDeltaTime(ticks);

    followPlaying();
    int type;
    while ( m_playingDeltaTime >= m_leadLagAdjust)
    {
        type = m_nextMidiEvent.type();

        if (type == MIDI_PB_EOF)
        {
            ppLogInfo("The End of the song");
            setEventBits(EVENT_BITS_playingStopped);
            setTransportPlaying(m_transport, false);
            break;
        }

        if (type == MIDI_PB_tempo)
        {
            m_tempo.setMidiTempo(m_nextMidiEvent.data1());
            m_leadLagAdjust = m_tempo.mSecToTicks( -getLatencyFix() );
        }
        else if (type == MIDI_PB_timeSignature)
        {
            m_bar.setTimeSig(m_nextMidiEvent.data1(), m_nextMidiEvent.data2());
            ppLogDebug("MIDI Time Signature %d/%d", m_nextMidiEvent.data1(),m_nextMidiEvent.data2());

        }
        else if ( type != MIDI_NONE )   // this marks the end of the piece of music
        {
            int channel = m_nextMidiEvent.channel();

            // Is this channel_muted
            if (!hasPianistKeyboardChannel(channel))
            {
                if (getfollowState() >= PB_FOLLOW_earlyNotes &&
                        (m_playMode == PB_PLAY_MODE_followYou || m_playMode == PB_PLAY_MODE_rhythmTapping) &&
                        !seekingBarNumber() &&
                        m_followSkillAdvanced == false)
                {
                    // Save up the notes until the pianist presses the right key
                    if (m_savedNoteQueue.space()>0)
                        m_savedNoteQueue.push(m_nextMidiEvent);
                    else
                        ppLogWarn("Warning the m_savedNoteQueue is full");

                    if (type == MIDI_NOTE_OFF)
                    {
                        if (m_savedNoteOffQueue.space()>0)
                            m_savedNoteOffQueue.push(m_nextMidiEvent);
                        else
                            ppLogDebug("Warning the m_savedNoteOffQueue is full");
                    }
                }
                else
                {
                    playTransposeEvent(m_nextMidiEvent); // Play the midi note or event
                    ppDEBUG_CONDUCTOR(("playEvent() chan %d type %d note %d", m_nextMidiEvent.channel(), m_nextMidiEvent.type(), m_nextMidiEvent.note()));
                }
            }
        }
        if (!readNextPlaybackEvent(m_nextMidiEvent))
        {
            ppDEBUG_CONDUCTOR(("no data in song queue"));
            m_nextMidiEvent.clear();
            break;
        }

        m_playingDeltaTime -= m_nextMidiEvent.deltaTime() * SPEED_ADJUST_FACTOR;
        followPlaying();
    }
}

void CConductor::rewind()
{
    int chan;

    m_realTimeEventBits = 0;
    for ( chan = 0; chan < MAX_MIDI_CHANNELS; chan++)
    {
        m_savedMainVolume[chan] = 100;
    }
    m_lastSound = -1;
    m_rating.reset();
    m_rating.setPlayAlongMode(m_playMode == PB_PLAY_MODE_playAlong);
    m_playingDeltaTime = 0;
    setTransportCurrentTick(m_transport, 0);
    resetCurrentSongTickRemainder();
    setPlaybackReadPosition(0);
    m_tempo.reset();

    m_savedNoteQueue.clear();
    m_savedNoteOffQueue.clear();
    m_nextMidiEvent.clear();
    m_bar.rewind();

    m_goodPlayedNotes.clear();  // The good notes the pianist plays
    clearPracticeState(&m_practiceState);
    clearMutedSplitNotes();
    if (m_piano)
        m_piano->clear();
    setChordTimelinePosition(0);
    setFollowSkillAdvanced(false);

    m_cfg_earlyNotesPoint = CMidiFile::ppqnAdjust(15); // was 10 playZoneEarly
    m_cfg_stopPointBeginner = CMidiFile::ppqnAdjust(-0); //was -3; // stop just after the beat
    m_cfg_stopPointAdvanced = CMidiFile::ppqnAdjust(-15); //was -3; // stop just after the beat
    m_cfg_imminentNotesOffPoint = CMidiFile::ppqnAdjust(-15);  // look ahead and find an Notes off coming up

    // Annie song 25

    m_cfg_playZoneEarly = CMidiFile::ppqnAdjust(static_cast<float>(Cfg::playZoneEarly())) * SPEED_ADJUST_FACTOR; // when playing along
    m_cfg_playZoneLate = CMidiFile::ppqnAdjust(static_cast<float>(Cfg::playZoneLate())) * SPEED_ADJUST_FACTOR;
}

void CConductor::init2(CScore * scoreWin, CSettings* settings)
{
    m_scoreWin = scoreWin;
    m_settings = settings;
    m_midiDevice.setQSettings(settings);

    setFollowSkillAdvanced(false);

    m_followState = PB_FOLLOW_searching;
    m_midiDevice.init();

    assert(m_scoreWin);
    if (m_scoreWin)
    {
        m_scoreWin->setRatingObject(&m_rating);
        m_piano = m_scoreWin->getPianoObject();
    }

    rewind();
}
