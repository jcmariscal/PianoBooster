#ifndef __CHORD_ANNOTATION_PLAYBACK_H__
#define __CHORD_ANNOTATION_PLAYBACK_H__

#include "SongData.h"

constexpr int AnnotatedChordPlaybackVelocity = 52;
constexpr int AnnotatedChordPlaybackVolume = 82;

enum AnnotatedChordPlayMode
{
    AnnotatedChordPlayRootChord,
    AnnotatedChordPlayComping,
    AnnotatedChordPlayProComping
};

constexpr int AnnotatedChordProStyleSimple = 1 << 0;
constexpr int AnnotatedChordProStyleBlues = 1 << 1;
constexpr int AnnotatedChordProStyleStride = 1 << 2;
constexpr int AnnotatedChordProStyleGospel = 1 << 3;
constexpr int AnnotatedChordProStyleFunk = 1 << 4;
constexpr int AnnotatedChordProStyleSoul = 1 << 5;
constexpr int AnnotatedChordProStyleClassical = 1 << 6;
constexpr int AnnotatedChordProStyleWaltz = 1 << 7;
constexpr int AnnotatedChordProStyleMarch = 1 << 8;
constexpr int AnnotatedChordProStyleBallad = 1 << 9;
constexpr int AnnotatedChordProStyleLatin = 1 << 10;
constexpr int AnnotatedChordProStyleReggae = 1 << 11;
constexpr int AnnotatedChordProStyleCountry = 1 << 12;
constexpr int AnnotatedChordProStyleNewAge = 1 << 13;
constexpr int AnnotatedChordProStyleRockBallad = 1 << 14;
constexpr int AnnotatedChordProStyleContemporaryJazz = 1 << 15;
constexpr int AnnotatedChordProStyleAll =
        AnnotatedChordProStyleSimple | AnnotatedChordProStyleBlues |
        AnnotatedChordProStyleStride | AnnotatedChordProStyleGospel |
        AnnotatedChordProStyleFunk | AnnotatedChordProStyleSoul |
        AnnotatedChordProStyleClassical | AnnotatedChordProStyleWaltz |
        AnnotatedChordProStyleMarch | AnnotatedChordProStyleBallad |
        AnnotatedChordProStyleLatin | AnnotatedChordProStyleReggae |
        AnnotatedChordProStyleCountry | AnnotatedChordProStyleNewAge |
        AnnotatedChordProStyleRockBallad | AnnotatedChordProStyleContemporaryJazz;

int annotatedChordPlaybackChannel(const SongData& song);
int annotatedChordPlaybackChannel(const SongData& song,
                                  const QVector<int>& excludedChannels);
QVector<int> annotatedChordPitches(const ChordAnnotation& annotation);
QVector<MidiEventRecord> buildAnnotatedChordPlaybackEvents(const SongData& song,
                                                           int channel,
                                                           int velocity);
QVector<MidiEventRecord> buildAnnotatedChordPlaybackEvents(const SongData& song,
                                                           int channel,
                                                           int velocity,
                                                           AnnotatedChordPlayMode mode);
QVector<MidiEventRecord> buildAnnotatedChordPlaybackEvents(const SongData& song,
                                                           int channel,
                                                           int velocity,
                                                           AnnotatedChordPlayMode mode,
                                                           int proStyleMask);
QVector<MidiEventRecord> buildPlaybackEventsWithAnnotatedChords(const SongData& song,
                                                                bool enabled);
QVector<MidiEventRecord> buildPlaybackEventsWithAnnotatedChords(const SongData& song,
                                                                bool enabled,
                                                                int channel);
QVector<MidiEventRecord> buildPlaybackEventsWithAnnotatedChords(const SongData& song,
                                                                bool enabled,
                                                                int channel,
                                                                AnnotatedChordPlayMode mode);
QVector<MidiEventRecord> buildPlaybackEventsWithAnnotatedChords(const SongData& song,
                                                                bool enabled,
                                                                int channel,
                                                                AnnotatedChordPlayMode mode,
                                                                int proStyleMask);

#endif
