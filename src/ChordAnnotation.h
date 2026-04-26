#ifndef __CHORD_ANNOTATION_H__
#define __CHORD_ANNOTATION_H__

#include <QString>
#include <QtGlobal>

constexpr int ChordAnnotationAutoKeySignature = 1000;

enum ChordAnnotationDetail
{
    ChordAnnotationBasic,
    ChordAnnotationSevenths,
    ChordAnnotationExtensions
};

enum ChordAnnotationLowConfidenceMode
{
    ChordAnnotationHideLowConfidence,
    ChordAnnotationShowConservative,
    ChordAnnotationShowAll
};

struct ChordAnnotation
{
    int barIndex = -1;
    qint64 startTick = 0;
    qint64 endTick = 0;
    QString label;
    QString suffix;
    int rootPitchClass = -1;
    int bassPitchClass = -1;
    float confidence = 0.0f;
};

struct ChordAnnotationOptions
{
    bool useSmoothing = true;
    bool carryEmptyBars = false;
    int keySignature = ChordAnnotationAutoKeySignature;
    int sourceChannel = -1;
    int sourceTrack = -1;
    ChordAnnotationDetail detail = ChordAnnotationExtensions;
    ChordAnnotationLowConfidenceMode lowConfidenceMode = ChordAnnotationHideLowConfidence;
    float minConfidence = 0.08f;
};

int normalizedPitchClass(int pitch);
QString chordPitchClassName(int pitchClass, int keySignature);
QString formatChordSymbol(int rootPitchClass, const QString& suffix,
                          int bassPitchClass, int keySignature);
QString transposedChordAnnotationLabel(const ChordAnnotation& annotation,
                                       int semitones, int keySignature);

#endif
