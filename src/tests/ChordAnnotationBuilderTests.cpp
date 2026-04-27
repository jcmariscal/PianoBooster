#include <iostream>
#include <initializer_list>

#include "ChordAnnotationBuilder.h"

namespace {
int failures = 0;

void expectString(const char *name, const QString& actual, const QString& expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected.toStdString()
              << ", got " << actual.toStdString() << '\n';
    failures++;
}

void expectBool(const char *name, bool actual, bool expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

void expectInt(const char *name, int actual, int expected)
{
    if (actual == expected)
        return;
    std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
    failures++;
}

NoteEvent note(int pitch, qint64 start, qint64 duration, int channel = 0)
{
    NoteEvent event;
    event.id = pitch;
    event.startTick = start;
    event.endTick = start + duration;
    event.pitch = pitch;
    event.velocity = 96;
    event.channel = channel;
    event.track = channel;
    return event;
}

BarMap fourFourBars(int bars)
{
    QVector<TimeSignatureChange> signatures;
    signatures.append(TimeSignatureChange());
    return buildBarMap(SongDataDefaultPpqn, bars * 4 * SongDataDefaultPpqn, signatures);
}

QString firstLabel(const SongData& song, int keySignature = -5)
{
    ChordAnnotationOptions options;
    options.keySignature = keySignature;
    options.useSmoothing = false;
    const QVector<ChordAnnotation> annotations = buildChordAnnotations(song, fourFourBars(1), options);
    return annotations.isEmpty() ? QString() : annotations.first().label;
}

QVector<ChordAnnotation> annotationsFor(const SongData& song, int bars,
                                        ChordAnnotationOptions options)
{
    return buildChordAnnotations(song, fourFourBars(bars), options);
}

SongData oneBarSong(std::initializer_list<int> pitches)
{
    SongData song;
    song.durationTicks = 4 * SongDataDefaultPpqn;
    for (int pitch : pitches)
        song.notes.append(note(pitch, 0, song.durationTicks));
    return song;
}

void appendBarChord(SongData& song, int bar, std::initializer_list<int> pitches)
{
    const qint64 start = bar * 4 * SongDataDefaultPpqn;
    for (int pitch : pitches)
        song.notes.append(note(pitch, start, 4 * SongDataDefaultPpqn));
}

void testFormatting()
{
    expectString("flat root", chordPitchClassName(1, -5), QStringLiteral("Db"));
    expectString("sharp root", chordPitchClassName(1, 3), QStringLiteral("C#"));
    expectString("slash chord", formatChordSymbol(3, QStringLiteral("m7"), 1, -5),
                 QStringLiteral("Ebm7/Db"));
    ChordAnnotation annotation;
    annotation.label = QStringLiteral("Cm7/G");
    annotation.suffix = QStringLiteral("m7");
    annotation.rootPitchClass = 0;
    annotation.bassPitchClass = 7;
    expectString("transposed annotation", transposedChordAnnotationLabel(annotation, 2, 3),
                 QStringLiteral("Dm7/A"));
}

void testCoreChordLabels()
{
    expectString("Dbmaj7", firstLabel(oneBarSong({49, 53, 56, 60})), QStringLiteral("Dbmaj7"));
    expectString("Fm7", firstLabel(oneBarSong({53, 56, 60, 63})), QStringLiteral("Fm7"));
    expectString("Gbmaj7", firstLabel(oneBarSong({54, 58, 61, 65})), QStringLiteral("Gbmaj7"));
}

void testAlteredDominant()
{
    SongData song = oneBarSong({48, 52, 56, 58, 63});
    expectString("C altered dominant", firstLabel(song, -5), QStringLiteral("C7#9(b13)"));
}

void testSlashChord()
{
    SongData song = oneBarSong({49, 51, 54, 58, 61});
    expectString("Ebm7 slash Db", firstLabel(song, -5), QStringLiteral("Ebm7/Db"));
}

void testPassingToneDoesNotDominate()
{
    SongData song = oneBarSong({48, 52, 55});
    song.notes.append(note(50, SongDataDefaultPpqn, 8));
    expectString("passing tone", firstLabel(song, 0), QStringLiteral("C"));
}

void testDrumsIgnored()
{
    SongData song;
    song.durationTicks = 4 * SongDataDefaultPpqn;
    song.notes.append(note(48, 0, song.durationTicks, MIDI_DRUM_CHANNEL));
    expectString("drum ignored", firstLabel(song, 0), QString());
}

void testCarryEmptyBars()
{
    SongData song;
    song.durationTicks = 8 * SongDataDefaultPpqn;
    song.notes.append(note(48, 0, 4 * SongDataDefaultPpqn));
    song.notes.append(note(52, 0, 4 * SongDataDefaultPpqn));
    song.notes.append(note(55, 0, 4 * SongDataDefaultPpqn));
    ChordAnnotationOptions options;
    options.keySignature = 0;
    options.carryEmptyBars = true;
    const QVector<ChordAnnotation> annotations = buildChordAnnotations(song, fourFourBars(2), options);
    expectBool("carry has two bars", annotations.size() == 2, true);
    expectString("first carried source", annotations[0].label, QStringLiteral("C"));
    expectString("empty bar carried", annotations[1].label, QStringLiteral("C"));
}

void testSustainedChordAcrossBars()
{
    SongData song;
    song.durationTicks = 8 * SongDataDefaultPpqn;
    song.notes.append(note(48, 0, song.durationTicks));
    song.notes.append(note(52, 0, song.durationTicks));
    song.notes.append(note(55, 0, song.durationTicks));
    const QVector<ChordAnnotation> annotations = buildChordAnnotations(song, fourFourBars(2));
    expectString("sustained first bar", annotations[0].label, QStringLiteral("C"));
    expectString("sustained second bar", annotations[1].label, QStringLiteral("C"));
}

void testStrongChangeSurvivesSmoothing()
{
    SongData song;
    song.durationTicks = 8 * SongDataDefaultPpqn;
    song.notes.append(note(48, 0, 4 * SongDataDefaultPpqn));
    song.notes.append(note(52, 0, 4 * SongDataDefaultPpqn));
    song.notes.append(note(55, 0, 4 * SongDataDefaultPpqn));
    song.notes.append(note(50, 4 * SongDataDefaultPpqn, 4 * SongDataDefaultPpqn));
    song.notes.append(note(54, 4 * SongDataDefaultPpqn, 4 * SongDataDefaultPpqn));
    song.notes.append(note(57, 4 * SongDataDefaultPpqn, 4 * SongDataDefaultPpqn));
    const QVector<ChordAnnotation> annotations = buildChordAnnotations(song, fourFourBars(2));
    expectString("strong change first", annotations[0].label, QStringLiteral("C"));
    expectString("strong change second", annotations[1].label, QStringLiteral("D"));
}

void testDetailLevels()
{
    ChordAnnotationOptions options;
    SongData song = oneBarSong({48, 52, 56, 58, 63});
    options.keySignature = -5;
    options.detail = ChordAnnotationSevenths;
    expectString("seventh detail", annotationsFor(song, 1, options)[0].label, QStringLiteral("C7"));
    options.detail = ChordAnnotationBasic;
    expectString("basic detail", annotationsFor(oneBarSong({49, 53, 56, 60}), 1, options)[0].label,
                 QStringLiteral("Db"));
    expectString("basic dominant detail", annotationsFor(song, 1, options)[0].label,
                 QStringLiteral("C"));
}

void testLowConfidenceModes()
{
    ChordAnnotationOptions options;
    SongData song = oneBarSong({48});
    options.minConfidence = 1.0f;
    expectString("hide low confidence", annotationsFor(song, 1, options)[0].label, QString());
    options.lowConfidenceMode = ChordAnnotationShowConservative;
    expectString("conservative low confidence", annotationsFor(song, 1, options)[0].label,
                 QStringLiteral("C"));
    options.lowConfidenceMode = ChordAnnotationShowAll;
    expectBool("show all low confidence", !annotationsFor(song, 1, options)[0].label.isEmpty(), true);
}

void testSourceChannelFilter()
{
    SongData song;
    song.durationTicks = 4 * SongDataDefaultPpqn;
    song.notes.append(note(48, 0, song.durationTicks, 0));
    song.notes.append(note(52, 0, song.durationTicks, 0));
    song.notes.append(note(55, 0, song.durationTicks, 0));
    song.notes.append(note(53, 0, song.durationTicks, 1));
    song.notes.append(note(57, 0, song.durationTicks, 1));
    song.notes.append(note(60, 0, song.durationTicks, 1));
    ChordAnnotationOptions options;
    options.sourceChannel = 1;
    const QVector<ChordAnnotation> annotations = buildChordAnnotations(song, fourFourBars(1), options);
    expectString("source channel", annotations.first().label, QStringLiteral("F"));
}

void testIntraBarSegmentation()
{
    SongData song;
    song.durationTicks = 4 * SongDataDefaultPpqn;
    for (int pitch : {48, 52, 55})
        song.notes.append(note(pitch, 0, 2 * SongDataDefaultPpqn));
    for (int pitch : {55, 59, 62, 65})
        song.notes.append(note(pitch, 2 * SongDataDefaultPpqn, 2 * SongDataDefaultPpqn));
    ChordAnnotationOptions options;
    options.keySignature = 0;
    options.useSmoothing = false;
    options.intraBarSegmentation = true;
    options.maxSegmentsPerBar = 2;
    const QVector<ChordAnnotation> annotations = buildChordAnnotations(song, fourFourBars(1), options);
    expectInt("intra-bar split count", annotations.size(), 2);
    expectString("intra-bar first", annotations[0].label, QStringLiteral("C"));
    expectString("intra-bar second", annotations[1].label, QStringLiteral("G7"));
    expectBool("intra-bar second start", annotations[1].startTick == 2 * SongDataDefaultPpqn, true);
}

void testPassingToneDoesNotSplitBar()
{
    SongData song = oneBarSong({48, 52, 55});
    song.notes.append(note(50, 2 * SongDataDefaultPpqn, 12));
    ChordAnnotationOptions options;
    options.keySignature = 0;
    options.useSmoothing = false;
    options.intraBarSegmentation = true;
    options.maxSegmentsPerBar = 2;
    const QVector<ChordAnnotation> annotations = buildChordAnnotations(song, fourFourBars(1), options);
    expectInt("passing tone split count", annotations.size(), 1);
    expectString("passing tone unsplit", annotations[0].label, QStringLiteral("C"));
}

void testMaxSegmentsPerBar()
{
    SongData song;
    song.durationTicks = 4 * SongDataDefaultPpqn;
    for (int pitch : {48, 52, 55})
        song.notes.append(note(pitch, 0, SongDataDefaultPpqn));
    for (int pitch : {53, 57, 60})
        song.notes.append(note(pitch, SongDataDefaultPpqn, SongDataDefaultPpqn));
    for (int pitch : {55, 59, 62})
        song.notes.append(note(pitch, 2 * SongDataDefaultPpqn, 2 * SongDataDefaultPpqn));
    ChordAnnotationOptions options;
    options.keySignature = 0;
    options.useSmoothing = false;
    options.intraBarSegmentation = true;
    options.maxSegmentsPerBar = 3;
    const QVector<ChordAnnotation> annotations = buildChordAnnotations(song, fourFourBars(1), options);
    expectInt("max segment count", annotations.size(), 3);
    expectString("max segment first", annotations[0].label, QStringLiteral("C"));
    expectString("max segment second", annotations[1].label, QStringLiteral("F"));
    expectString("max segment third", annotations[2].label, QStringLiteral("G"));
}

void testStableModeDominantChain()
{
    SongData song;
    song.durationTicks = 20 * SongDataDefaultPpqn;
    appendBarChord(song, 0, {52, 56, 59});
    appendBarChord(song, 1, {45, 49, 52});
    appendBarChord(song, 2, {50, 54, 57});
    appendBarChord(song, 3, {43, 47, 50});
    appendBarChord(song, 4, {48, 52, 55});
    ChordAnnotationOptions options;
    options.keySignature = 0;
    options.mode = ChordAnnotationStableMidiProfile;
    const QVector<ChordAnnotation> annotations = buildChordAnnotations(song, fourFourBars(5), options);
    expectInt("stable dominant count", annotations.size(), 5);
    expectString("stable dominant E", annotations[0].label, QStringLiteral("E7"));
    expectString("stable dominant A", annotations[1].label, QStringLiteral("A7"));
    expectString("stable dominant D", annotations[2].label, QStringLiteral("D7"));
    expectString("stable dominant G", annotations[3].label, QStringLiteral("G7"));
    expectString("stable dominant C", annotations[4].label, QStringLiteral("C"));
}

void testStableModeMergesShortReturnBlip()
{
    SongData song;
    song.durationTicks = 4 * SongDataDefaultPpqn;
    for (int pitch : {48, 52, 55})
        song.notes.append(note(pitch, 0, SongDataDefaultPpqn));
    for (int pitch : {50, 54, 57})
        song.notes.append(note(pitch, SongDataDefaultPpqn, SongDataDefaultPpqn));
    for (int pitch : {48, 52, 55})
        song.notes.append(note(pitch, 2 * SongDataDefaultPpqn, 2 * SongDataDefaultPpqn));
    ChordAnnotationOptions options;
    options.keySignature = 0;
    options.mode = ChordAnnotationStableMidiProfile;
    options.useSmoothing = false;
    options.intraBarSegmentation = true;
    options.maxSegmentsPerBar = 3;
    const QVector<ChordAnnotation> annotations = buildChordAnnotations(song, fourFourBars(1), options);
    expectInt("stable blip count", annotations.size(), 1);
    expectString("stable blip label", annotations[0].label, QStringLiteral("C"));
}
}

int main()
{
    testFormatting();
    testCoreChordLabels();
    testAlteredDominant();
    testSlashChord();
    testPassingToneDoesNotDominate();
    testDrumsIgnored();
    testCarryEmptyBars();
    testSustainedChordAcrossBars();
    testStrongChangeSurvivesSmoothing();
    testDetailLevels();
    testLowConfidenceModes();
    testSourceChannelFilter();
    testIntraBarSegmentation();
    testPassingToneDoesNotSplitBar();
    testMaxSegmentsPerBar();
    testStableModeDominantChain();
    testStableModeMergesShortReturnBlip();

    if (failures == 0) {
        std::cout << "ChordAnnotationBuilder tests passed\n";
        return 0;
    }
    std::cerr << failures << " chord annotation test(s) failed\n";
    return 1;
}
