#ifndef PIANOBOOSTER_SONG_DATA_H
#define PIANOBOOSTER_SONG_DATA_H

#include <QVector>
#include <QString>
#include <QtGlobal>

#include "ChordAnnotation.h"
#include "MidiEvent.h"

constexpr int SongDataDefaultPpqn = 96;

struct SongMetadata
{
    QString fileName;
    QString title;
    int trackCount = 0;
};

struct MidiEventRecord
{
    CMidiEvent event;
    qint64 absoluteTick = 0;
    qint64 deltaTick = 0;
    int streamIndex = -1;
    int track = -1;
    int channel = -1;
};

struct TempoChange
{
    qint64 tick = 0;
    int microsecondsPerQuarter = 500000;
};

struct TimeSignatureChange
{
    qint64 tick = 0;
    int numerator = 4;
    int denominator = 4;
};

struct NoteEvent
{
    int id = -1;
    qint64 startTick = 0;
    qint64 endTick = 0;
    int pitch = -1;
    int velocity = 0;
    int channel = -1;
    int track = -1;
    int hand = -1;
};

struct SongData
{
    SongMetadata metadata;
    int ppqn = SongDataDefaultPpqn;
    qint64 durationTicks = 0;
    QVector<MidiEventRecord> events;
    QVector<int> eventIndexesByChannel[MAX_MIDI_CHANNELS];
    QVector<QVector<int>> eventIndexesByTrack;
    QVector<NoteEvent> notes;
    QVector<TempoChange> tempos;
    QVector<TimeSignatureChange> timeSignatures;
    QVector<ChordAnnotation> chordAnnotations;
};

#endif
