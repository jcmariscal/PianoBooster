#include "ChordAnnotation.h"

int normalizedPitchClass(int pitch)
{
    int value = pitch % 12;
    return value < 0 ? value + 12 : value;
}

QString chordPitchClassName(int pitchClass, int keySignature)
{
    static const char *sharpNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    static const char *flatNames[] = {
        "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
    };
    const int pc = normalizedPitchClass(pitchClass);
    if (keySignature < 0)
        return QString::fromLatin1(flatNames[pc]);
    if (keySignature > 0)
        return QString::fromLatin1(sharpNames[pc]);
    return QString::fromLatin1((pc == 6) ? sharpNames[pc] : flatNames[pc]);
}

QString formatChordSymbol(int rootPitchClass, const QString& suffix,
                          int bassPitchClass, int keySignature)
{
    if (rootPitchClass < 0)
        return QString();
    QString label = chordPitchClassName(rootPitchClass, keySignature) + suffix;
    if (bassPitchClass >= 0 && normalizedPitchClass(bassPitchClass) != normalizedPitchClass(rootPitchClass))
        label += QStringLiteral("/") + chordPitchClassName(bassPitchClass, keySignature);
    return label;
}

QString transposedChordAnnotationLabel(const ChordAnnotation& annotation,
                                       int semitones, int keySignature)
{
    if (annotation.label.isEmpty() || annotation.rootPitchClass < 0)
        return QString();
    const int root = normalizedPitchClass(annotation.rootPitchClass + semitones);
    const int bass = annotation.bassPitchClass >= 0 ?
                normalizedPitchClass(annotation.bassPitchClass + semitones) : -1;
    return formatChordSymbol(root, annotation.suffix, bass, keySignature);
}
