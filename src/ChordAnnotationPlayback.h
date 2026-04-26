#ifndef __CHORD_ANNOTATION_PLAYBACK_H__
#define __CHORD_ANNOTATION_PLAYBACK_H__

#include "SongData.h"

constexpr int AnnotatedChordPlaybackVelocity = 52;
constexpr int AnnotatedChordPlaybackVolume = 82;

int annotatedChordPlaybackChannel(const SongData& song);
int annotatedChordPlaybackChannel(const SongData& song,
                                  const QVector<int>& excludedChannels);
QVector<int> annotatedChordPitches(const ChordAnnotation& annotation);
QVector<MidiEventRecord> buildAnnotatedChordPlaybackEvents(const SongData& song,
                                                           int channel,
                                                           int velocity);
QVector<MidiEventRecord> buildPlaybackEventsWithAnnotatedChords(const SongData& song,
                                                                bool enabled);
QVector<MidiEventRecord> buildPlaybackEventsWithAnnotatedChords(const SongData& song,
                                                                bool enabled,
                                                                int channel);

#endif
