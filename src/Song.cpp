/*********************************************************************************/
/*!
@file           Song.cpp

@brief          xxxxx.

@author         L. J. Barman

    Copyright (c)   2008-2020, L. J. Barman and others, all rights reserved

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

#include "Song.h"
#include "ChordAnnotationPlayback.h"
#include "ChordAnnotationBuilder.h"
#include "Score.h"
#include "Settings.h"

#include <algorithm>
#include <limits>

namespace {
int validChannel(int channel)
{
    if (channel < 0 || channel >= MAX_MIDI_CHANNELS)
        return -1;
    return channel;
}

int validTrack(int track, int trackCount)
{
    if (track < 0 || track >= trackCount)
        return -1;
    return track;
}

bool isChannelEvent(int type)
{
    return type == MIDI_NOTE_OFF || type == MIDI_NOTE_ON ||
            type == MIDI_NOTE_PRESSURE || type == MIDI_CONTROL_CHANGE ||
            type == MIDI_PROGRAM_CHANGE || type == MIDI_CHANNEL_PRESSURE ||
            type == MIDI_PITCH_BEND;
}

int recordChannel(CMidiEvent event)
{
    if (!isChannelEvent(event.type()))
        return -1;
    return validChannel(event.channel());
}

int recordTrack(CMidiEvent event, int trackCount)
{
    if (event.type() == MIDI_NONE || event.type() == MIDI_PB_EOF)
        return -1;
    return validTrack(event.track(), trackCount);
}

bool validPitch(int pitch)
{
    return pitch >= 0 && pitch < MAX_MIDI_NOTES;
}

void clearActiveNotes(int active[MAX_MIDI_CHANNELS][MAX_MIDI_NOTES])
{
    for (int channel = 0; channel < MAX_MIDI_CHANNELS; channel++)
        for (int pitch = 0; pitch < MAX_MIDI_NOTES; pitch++)
            active[channel][pitch] = -1;
}

void closeNote(QVector<NoteEvent>& notes, int index, qint64 endTick)
{
    if (index < 0 || index >= notes.size())
        return;
    if (endTick < notes[index].startTick)
        endTick = notes[index].startTick;
    notes[index].endTick = endTick;
}

void appendTempoChange(QVector<TempoChange>& tempos, qint64 tick, int value)
{
    if (value <= 0)
        return;
    TempoChange change;
    change.tick = tick;
    change.microsecondsPerQuarter = value;
    if (!tempos.isEmpty() && tempos.last().tick == tick)
        tempos.last() = change;
    else
        tempos.append(change);
}

void appendTimeSignatureChange(QVector<TimeSignatureChange>& signatures,
                               qint64 tick, int numerator, int denominator)
{
    if (numerator <= 0 || denominator <= 0)
        return;
    TimeSignatureChange change;
    change.tick = tick;
    change.numerator = numerator;
    change.denominator = denominator;
    if (!signatures.isEmpty() && signatures.last().tick == tick)
        signatures.last() = change;
    else
        signatures.append(change);
}

int eventDelta(qint64 fromTick, qint64 toTick)
{
    qint64 delta = toTick - fromTick;
    if (delta < 0)
        delta = 0;
    if (delta > std::numeric_limits<int>::max())
        return std::numeric_limits<int>::max();
    return static_cast<int>(delta);
}

qint64 boundedTick(qint64 tick, qint64 duration)
{
    if (tick < 0)
        return 0;
    if (tick > duration)
        return duration;
    return tick;
}

ChordAnnotationDetail validChordAnnotationDetail(int value)
{
    if (value == ChordAnnotationBasic || value == ChordAnnotationSevenths)
        return static_cast<ChordAnnotationDetail>(value);
    return ChordAnnotationExtensions;
}

ChordAnnotationLowConfidenceMode validLowConfidenceMode(int value)
{
    if (value == ChordAnnotationShowConservative || value == ChordAnnotationShowAll)
        return static_cast<ChordAnnotationLowConfidenceMode>(value);
    return ChordAnnotationHideLowConfidence;
}

ChordAnnotationMode validChordAnnotationMode(int value)
{
    if (value == ChordAnnotationNaive || value == ChordAnnotationStableMidiProfile)
        return static_cast<ChordAnnotationMode>(value);
    return ChordAnnotationNaive;
}

AnnotatedChordPlayMode validAnnotatedChordPlayMode(int value)
{
    if (value == AnnotatedChordPlayRootChord || value == AnnotatedChordPlayComping ||
            value == AnnotatedChordPlayProComping)
        return static_cast<AnnotatedChordPlayMode>(value);
    return AnnotatedChordPlayRootChord;
}

float validConfidence(double value)
{
    if (value < 0.0)
        return 0.0f;
    if (value > 1.0)
        return 1.0f;
    return static_cast<float>(value);
}

int validMaxChordSegmentsPerBar(int value)
{
    if (value < 1)
        return 1;
    if (value > 4)
        return 4;
    return value;
}
}

void CSong::init2(CScore * scoreWin, CSettings* settings)
{

    CNote::reset();
    m_scoreWin = scoreWin;
    m_settings = settings;

    m_conductor.init2(scoreWin, settings);

    setActiveHand(PB_PART_both);
    setPlayMode(PB_PLAY_MODE_followYou);
    setSpeed(1.0);
    setSkill(3);
}

void CSong::loadSong(const QString & filename)
{
    CNote::reset();

    m_songTitle = filename;
    CStavePos::setKeySignature(NOT_USED, 0);
    int index = m_songTitle.lastIndexOf("/");
    if (index >= 0)
        m_songTitle = m_songTitle.right( m_songTitle.length() - index - 1);

    QString fn = filename;
#ifdef _WIN32
     fn = fn.replace('/','\\');
#endif
    m_midiFile.setLogLevel(3);
    m_midiFile.openMidiFile(string(fn.toLocal8Bit().data()));
    resetSongData(filename);
    ppLogInfo("Opening song %s",  fn.toLocal8Bit().data());
    transpose(0);
    midiFileInfo();
    m_midiFile.setLogLevel(99);
    playMusic(false);
    rewind();
    setPlayFromBar(0.0);
    setLoopingBars(0.0);
    m_conductor.setEventBits(EVENT_BITS_loadSong);
    if (!m_midiFile.getSongTitle().isEmpty())
        m_songTitle = m_midiFile.getSongTitle();
    m_songData.metadata.title = m_songTitle;

}

void CSong::resetSongData(const QString &filename)
{
    m_songData = SongData();
    m_songData.metadata.fileName = filename;
    m_songData.metadata.title = m_songTitle;
    m_songData.metadata.trackCount = m_midiFile.numberOfTracks();
    m_songData.ppqn = CMidiFile::getPulsesPerQuarterNote();
    m_songData.eventIndexesByTrack.resize(m_songData.metadata.trackCount);
    m_playbackEvents.clear();
    m_annotatedChordPlaybackEnabled = false;
    m_annotatedChordPlaybackChannel = -1;
    m_conductor.setAnnotatedChordPlaybackChannel(-1);
    m_conductor.setPlaybackEvents(&m_playbackEvents);
    setSongDataReadPosition(0);
}

void CSong::appendSongDataEvent(CMidiEvent event, int streamIndex)
{
    MidiEventRecord record;
    record.event = event;
    record.absoluteTick = event.absoluteTime();
    record.deltaTick = event.deltaTime();
    if (record.absoluteTick < 0)
        record.absoluteTick = 0;
    if (record.deltaTick < 0)
        record.deltaTick = 0;
    record.streamIndex = streamIndex;
    record.track = recordTrack(event, m_songData.metadata.trackCount);
    record.channel = recordChannel(event);

    const int eventIndex = m_songData.events.size();
    m_songData.events.append(record);
    if (record.channel >= 0)
        m_songData.eventIndexesByChannel[record.channel].append(eventIndex);
    if (record.track >= 0)
        m_songData.eventIndexesByTrack[record.track].append(eventIndex);
    if (record.absoluteTick > m_songData.durationTicks)
        m_songData.durationTicks = record.absoluteTick;
}

int CSong::getBarNumber() const
{
    return barAtTick(m_barMap, m_conductor.currentSongTick());
}

double CSong::getCurrentBarPos() const
{
    return barPositionAtTick(m_barMap, m_conductor.currentSongTick());
}

void CSong::buildSongDataMaps()
{
    m_songData.tempos.clear();
    m_songData.timeSignatures.clear();
    appendTempoChange(m_songData.tempos, 0, 500000);
    appendTimeSignatureChange(m_songData.timeSignatures, 0, 4, 4);

    for (int i = 0; i < m_songData.events.size(); i++)
    {
        MidiEventRecord record = m_songData.events[i];
        if (record.event.type() == MIDI_PB_tempo)
            appendTempoChange(m_songData.tempos, record.absoluteTick, record.event.data1());
        else if (record.event.type() == MIDI_PB_timeSignature)
            appendTimeSignatureChange(m_songData.timeSignatures, record.absoluteTick,
                                      record.event.data1(), record.event.data2());
    }
}

void CSong::setSongDataReadPosition(qint64 tick)
{
    if (tick < 0)
        tick = 0;
    auto it = std::lower_bound(m_songData.events.begin(), m_songData.events.end(), tick,
                               [](const MidiEventRecord& record, qint64 value) {
        return record.absoluteTick < value;
    });
    m_songDataReadIndex = static_cast<int>(it - m_songData.events.begin());
    m_songDataReadTick = tick;
}

CMidiEvent CSong::readSongDataEvent()
{
    CMidiEvent event;
    if (m_songDataReadIndex < 0 || m_songDataReadIndex >= m_songData.events.size())
    {
        event.setType(MIDI_PB_EOF);
        return event;
    }

    const MidiEventRecord record = m_songData.events[m_songDataReadIndex++];
    event = record.event;
    event.setDeltaTime(eventDelta(m_songDataReadTick, record.absoluteTick));
    m_songDataReadTick = record.absoluteTick;
    return event;
}

void CSong::buildSongDataNotes()
{
    int active[MAX_MIDI_CHANNELS][MAX_MIDI_NOTES];
    clearActiveNotes(active);
    m_songData.notes.clear();

    for (int i = 0; i < m_songData.events.size(); i++)
    {
        MidiEventRecord record = m_songData.events[i];
        const int pitch = record.event.note();
        if (record.channel < 0 || !validPitch(pitch))
            continue;

        int& activeIndex = active[record.channel][pitch];
        if (record.event.type() == MIDI_NOTE_ON)
        {
            closeNote(m_songData.notes, activeIndex, record.absoluteTick);
            NoteEvent note;
            note.id = m_songData.notes.size();
            note.startTick = record.absoluteTick;
            note.endTick = record.absoluteTick + record.event.getDuration();
            note.pitch = pitch;
            note.velocity = record.event.velocity();
            note.channel = record.channel;
            note.track = record.track;
            note.hand = CNote::findHand(record.event, record.channel, PB_PART_both);
            m_songData.notes.append(note);
            activeIndex = note.id;
        }
        else if (record.event.type() == MIDI_NOTE_OFF)
        {
            closeNote(m_songData.notes, activeIndex, record.absoluteTick);
            activeIndex = -1;
        }
    }

    for (int channel = 0; channel < MAX_MIDI_CHANNELS; channel++)
        for (int pitch = 0; pitch < MAX_MIDI_NOTES; pitch++)
            closeNote(m_songData.notes, active[channel][pitch], m_songData.durationTicks);
}

// read the file ahead to collect info about the song first
void CSong::midiFileInfo()
{
    m_trackList.reset(m_midiFile.numberOfTracks());
    m_conductor.setTimeSig(0, 0);
    CStavePos::setKeySignature( NOT_USED, 0 );

    // Read the next events to find the active channels
    CMidiEvent event;
    int streamIndex = 0;
    while ( true )
    {
        event = m_midiFile.readMidiEvent();
        appendSongDataEvent(event, streamIndex++);
        m_trackList.examineMidiEvent(event);

        if (event.type() == MIDI_PB_timeSignature)
        {
            m_conductor.setTimeSig(event.data1(), event.data2());
        }

        if (event.type() == MIDI_PB_EOF)
            break;
    }
    buildSongDataMaps();
    m_barMap = buildBarMap(m_songData.ppqn, m_songData.durationTicks,
                           m_songData.timeSignatures);
    rebuildScoreData();
}

void CSong::rebuildScoreData()
{
    buildSongDataNotes();
    m_songData.chordAnnotations = buildChordAnnotations(m_songData, m_barMap,
                                                        chordAnnotationOptions());
    rebuildPlaybackEvents();
    if (m_scoreWin != nullptr)
        m_scoreWin->setSongData(m_songData);
    regenerateChordQueue();
    forceScoreRedraw();
}

void CSong::rebuildPlaybackEvents()
{
    const bool enabled = m_settings != nullptr &&
            m_settings->value("Song/PlayAnnotatedChords", false).toBool();
    const qint64 tick = m_conductor.currentSongTick();
    if (m_annotatedChordPlaybackEnabled)
        stopAnnotatedChordPlayback();
    m_annotatedChordPlaybackEnabled = enabled;
    m_annotatedChordPlaybackChannel = enabled ?
                annotatedChordPlaybackChannel(m_songData, annotatedChordBlockedChannels()) : -1;
    m_conductor.setAnnotatedChordPlaybackChannel(m_annotatedChordPlaybackChannel);
    m_playbackEvents = buildPlaybackEventsWithAnnotatedChords(
                m_songData, enabled, m_annotatedChordPlaybackChannel, annotatedChordPlayMode());
    m_conductor.setPlaybackEvents(&m_playbackEvents);
    m_conductor.setPlaybackReadPosition(tick);
    if (enabled)
        prepareAnnotatedChordPlaybackChannel();
    if (enabled && playingMusic())
        playAnnotatedChordAtTick(tick);
}

void CSong::updateAnnotatedChordPlaybackVolume()
{
    prepareAnnotatedChordPlaybackChannel();
}

ChordAnnotationOptions CSong::chordAnnotationOptions() const
{
    ChordAnnotationOptions options;
    if (m_settings == nullptr)
        return options;
    options.useSmoothing = m_settings->value("Song/AnnotateUseSmoothing", true).toBool();
    options.carryEmptyBars = m_settings->value("Song/AnnotateCarryEmptyBars", false).toBool();
    options.maxSegmentsPerBar = validMaxChordSegmentsPerBar(
                m_settings->value("Song/AnnotateMaxSegmentsPerBar", 1).toInt());
    options.intraBarSegmentation = options.maxSegmentsPerBar > 1;
    if (!options.intraBarSegmentation)
        options.maxSegmentsPerBar = 1;
    options.sourceChannel = m_settings->value("Song/AnnotateSourceChannel", -1).toInt();
    options.sourceTrack = m_settings->value("Song/AnnotateSourceTrack", -1).toInt();
    options.mode = validChordAnnotationMode(
                m_settings->value("Song/AnnotateChordMode", ChordAnnotationNaive).toInt());
    options.detail = validChordAnnotationDetail(
                m_settings->value("Song/AnnotateDetail", ChordAnnotationExtensions).toInt());
    options.lowConfidenceMode = validLowConfidenceMode(
                m_settings->value("Song/AnnotateLowConfidenceMode", ChordAnnotationHideLowConfidence).toInt());
    options.minConfidence = validConfidence(m_settings->value("Song/AnnotateMinConfidence", 0.08).toDouble());
    return options;
}

void CSong::rewind()
{
    m_midiFile.rewind();
    m_conductor.rewind();
    setSongDataReadPosition(0);
    m_conductor.setPlaybackReadPosition(0);
    m_scoreWin->seekToTick(0);
    reset();
    forceScoreRedraw();
}

void CSong::playFromStartBar()
{
    rewind();
    playMusic(true);
}

void CSong::playMusic(bool start)
{
    if (start)
        directSeekToPlayFromBar();
    m_conductor.playMusic(start);
    if (start)
    {
        restoreMidiStateAtTick(m_conductor.currentSongTick());
        prepareAnnotatedChordPlaybackChannel();
        playAnnotatedChordAtTick(m_conductor.currentSongTick());
    }
}

void CSong::setPianistChannels(int goodChan, int badChan)
{
    m_conductor.setPianistChannels(goodChan, badChan);
    if (m_annotatedChordPlaybackEnabled)
        rebuildPlaybackEvents();
}

void CSong::boostVolume(int value)
{
    m_conductor.boostVolume(value);
    prepareAnnotatedChordPlaybackChannel();
}

void CSong::pianoVolume(int value)
{
    m_conductor.pianoVolume(value);
    prepareAnnotatedChordPlaybackChannel();
}

void CSong::setPlayFromBar(double bar)
{
    setPlayFromTick(tickAtBarPosition(m_barMap, bar));
}

void CSong::setPlayFromTick(qint64 tick)
{
    tick = boundedTick(tick, m_songData.durationTicks);
    m_playFromBar = barPositionAtTick(m_barMap, tick);
    updateTransportLoop();
}

void CSong::setLoopingBars(double bars)
{
    if (bars < 0.0)
        bars = 0.0;
    m_loopingBars = bars;
    updateTransportLoop();
}

void CSong::stopAnnotatedChordPlayback()
{
    if (m_annotatedChordPlaybackChannel < 0)
        return;
    CMidiEvent midi;
    midi.controlChangeEvent(0, m_annotatedChordPlaybackChannel, MIDI_ALL_NOTES_OFF, 0);
    m_conductor.playMidiEvent(midi);
    midi.controlChangeEvent(0, m_annotatedChordPlaybackChannel, MIDI_SUSTAIN, 0);
    m_conductor.playMidiEvent(midi);
}

QVector<int> CSong::annotatedChordBlockedChannels() const
{
    QVector<int> channels;
    for (int channel = 0; channel < MAX_MIDI_CHANNELS; channel++)
        if (m_conductor.hasPianistKeyboardChannel(channel))
            channels.append(channel);
    return channels;
}

void CSong::prepareAnnotatedChordPlaybackChannel()
{
    if (!m_annotatedChordPlaybackEnabled || m_annotatedChordPlaybackChannel < 0)
        return;
    CMidiEvent midi;
    midi.programChangeEvent(0, m_annotatedChordPlaybackChannel, GM_PIANO_PATCH);
    m_conductor.playMidiEvent(midi);
    midi.controlChangeEvent(0, m_annotatedChordPlaybackChannel, MIDI_MAIN_VOLUME,
                            annotatedChordPlaybackVolume());
    m_conductor.playMidiEvent(midi);
}

int CSong::annotatedChordPlaybackVolume() const
{
    if (m_settings == nullptr)
        return AnnotatedChordPlaybackVolume;
    return qBound(0, m_settings->value("Song/AnnotatedChordVolume",
                                       AnnotatedChordPlaybackVolume).toInt(), 127);
}

AnnotatedChordPlayMode CSong::annotatedChordPlayMode() const
{
    if (m_settings == nullptr)
        return AnnotatedChordPlayRootChord;
    return validAnnotatedChordPlayMode(
                m_settings->value("Song/AnnotatedChordPlayMode",
                                  AnnotatedChordPlayRootChord).toInt());
}

void CSong::playAnnotatedChordAtTick(qint64 tick)
{
    if (!m_annotatedChordPlaybackEnabled || m_annotatedChordPlaybackChannel < 0)
        return;
    if (annotatedChordPlayMode() != AnnotatedChordPlayRootChord)
        return;
    for (const ChordAnnotation& annotation : m_songData.chordAnnotations)
    {
        if (annotation.startTick >= tick || tick >= annotation.endTick)
            continue;
        for (int pitch : annotatedChordPitches(annotation))
        {
            CMidiEvent midi;
            midi.noteOnEvent(0, m_annotatedChordPlaybackChannel, pitch,
                             AnnotatedChordPlaybackVelocity);
            if (getTranspose() != 0)
                midi.transpose(getTranspose());
            m_conductor.playMidiEvent(midi);
        }
    }
}

void CSong::updateTransportLoop()
{
    if (m_loopingBars <= 0.0)
    {
        m_conductor.setTransportLoopTicks(0, 0);
        return;
    }
    const qint64 startTick = tickAtBarPosition(m_barMap, m_playFromBar);
    const qint64 endTick = tickAtBarPosition(m_barMap, getPlayUptoBar());
    m_conductor.setTransportLoopTicks(startTick, endTick);
}

void CSong::seekToTick(qint64 tick)
{
    tick = boundedTick(tick, m_songData.durationTicks);
    if (tick == m_conductor.currentSongTick())
        return;
    if (tick < m_conductor.currentSongTick())
        rewind();

    m_reachedMidiEof = false;
    m_scoreWin->seekToTick(tick);
    setSongDataReadPosition(tick);
    m_conductor.setPlaybackReadPosition(tick);
    m_conductor.seekForwardToTick(tick);
    restoreMidiStateAtTick(tick);
    forceScoreRedraw();
}

void CSong::previewScoreTick(qint64 tick)
{
    tick = boundedTick(tick, m_songData.durationTicks);
    if (m_scoreWin != nullptr)
        m_scoreWin->seekToTick(tick);
    forceScoreRedraw();
}

void CSong::directSeekToPlayFromBar()
{
    const qint64 targetTick = tickAtBarPosition(m_barMap, m_playFromBar);
    if (targetTick > m_conductor.currentSongTick())
        seekToTick(targetTick);
}

void CSong::restoreMidiStateAtTick(qint64 tick)
{
    const MidiStateSnapshot state = buildMidiStateSnapshot(m_songData.events, tick);
    for (int channel = 0; channel < MAX_MIDI_CHANNELS; channel++)
        restoreBankSelectState(state.channels[channel]);
    for (int channel = 0; channel < MAX_MIDI_CHANNELS; channel++)
        restoreProgramState(state.channels[channel]);
    for (int channel = 0; channel < MAX_MIDI_CHANNELS; channel++)
        restoreControllerState(state.channels[channel]);
}

void CSong::restoreBankSelectState(const MidiChannelState& state)
{
    if (state.hasController[MidiBankSelectMsb])
        m_conductor.playSeekRestoreEvent(state.controllers[MidiBankSelectMsb]);
    if (state.hasController[MidiBankSelectLsb])
        m_conductor.playSeekRestoreEvent(state.controllers[MidiBankSelectLsb]);
}

void CSong::restoreProgramState(const MidiChannelState& state)
{
    if (state.hasProgram)
        m_conductor.playSeekRestoreEvent(state.program);
}

void CSong::restoreControllerState(const MidiChannelState& state)
{
    for (int controller = 0; controller < MidiControllerCount; controller++)
    {
        if (controller == MidiBankSelectMsb || controller == MidiBankSelectLsb)
            continue;
        if (state.hasController[controller])
            m_conductor.playSeekRestoreEvent(state.controllers[controller]);
    }
}

void CSong::setActiveHand(whichPart_t hand)
{
    if (hand < PB_PART_both)
        hand = PB_PART_both;
    if (hand > PB_PART_left)
        hand = PB_PART_left;

    m_conductor.setActiveHand(hand);
    regenerateChordQueue();

    m_scoreWin->setDisplayHand(hand);
}

void CSong::setActiveChannel(int chan)
{
    m_conductor.setActiveChannel(chan);
    m_scoreWin->setActiveChannel(chan);
    regenerateChordQueue();
}

void  CSong::setPlayMode(playMode_t mode)
{
    regenerateChordQueue();
    m_conductor.setPlayMode(mode);
    forceScoreRedraw();
}

void CSong::regenerateChordQueue()
{
    m_conductor.setChordTimeline(buildChordTimeline(m_songData, getActiveChannel(), PB_PART_both));
}

void CSong::invalidateActiveScoreCache()
{
    m_scoreWin->invalidateActiveScoreCache();
    forceScoreRedraw();
}

void CSong::invalidateScoreRendererCaches()
{
    m_scoreWin->invalidateRendererCaches();
    forceScoreRedraw();
}

eventBits_t CSong::task(qint64 ticks)
{
    m_conductor.realTimeEngine(ticks);
    return m_conductor.takeEventBits();
}

static const struct pcNote_s
{
    int key;
    int note;
} pcNoteLookup[] =
{
    { 'a', PC_KEY_LOWEST_NOTE },
    { 'z', 59 }, // B
    { 'x', 60 }, // Middle C
    { 'd', 61 },
    { 'c', 62 }, // D
    { 'f', 63 },
    { 'v', 64 }, // E
    { 'b', 65 }, // F
    { 'h', 66 },
    { 'n', 67 }, // G
    { 'j', 68 },
    { 'm', 69 }, // A
    { 'k', 70 },
    { ',', 71 }, // B
    { '.', 72 }, // C
    { ';', 73 },
    { '/', 74 }, // D
    { '\'', PC_KEY_HIGHEST_NOTE },
};

// Fakes a midi piano keyboard using the PC keyboard
bool CSong::pcKeyPress(int key, bool down)
{
    int i;
    CMidiEvent midi;
    const int cfg_pcKeyVolume = 64;
    const int cfg_pcKeyChannel = 1-1;

    if (key == 't') // the tab key on the PC fakes good notes
    {
        if (down)
            m_fakeChord = getWantedChord();
        for (i = 0; i < m_fakeChord.length(); i++)
        {
            if (down)
                midi.noteOnEvent(0, cfg_pcKeyChannel, m_fakeChord.getNote(i).pitch() + getTranspose(), cfg_pcKeyVolume);
            else
                midi.noteOffEvent(0, cfg_pcKeyChannel, m_fakeChord.getNote(i).pitch() + getTranspose(), cfg_pcKeyVolume);
            m_conductor.expandPianistInput(midi);
        }
        return true;
    }

    for (int j = 0; j < arraySize(pcNoteLookup); j++)
    {
        if ( key==pcNoteLookup[j].key)
        {
            if (down)
                midi.noteOnEvent(0, cfg_pcKeyChannel, pcNoteLookup[j].note, cfg_pcKeyVolume);
            else
                midi.noteOffEvent(0, cfg_pcKeyChannel, pcNoteLookup[j].note, cfg_pcKeyVolume);

            m_conductor.expandPianistInput(midi);
            return true;
        }
    }
    //printf("pcKeyPress %d %d\n", m_pcNote, key);
    return false;
}
