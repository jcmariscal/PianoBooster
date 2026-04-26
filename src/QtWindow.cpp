/*!
    @file           QtWindow.cpp

    @brief          xxx.

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

#include "GlView.h"
#include "QtWindow.h"
#include "version.h"
#include "Draw.h"

#include <QDebug>
#include <QSurfaceFormat>
#include <QStringBuilder>

namespace {
constexpr int ScoreScrollRange = 100000;
constexpr const char ChordConfigBasic[] = "basic";
constexpr const char ChordConfigSevenths[] = "sevenths";
constexpr const char ChordConfigLeadSheet[] = "lead-sheet";
constexpr const char ChordConfigLeadSheetRepeats[] = "lead-sheet-repeats";
constexpr const char ChordConfigSetting[] = "Song/AnnotateChordConfiguration";
constexpr int ChordConfigMaxSegmentsLimit = 4;
constexpr int ClusterNormalMaxSpanDefault = MIDI_OCTAVE;
constexpr int ClusterWideMaxSpanDefault = MIDI_OCTAVE + 4;
constexpr int ClusterMaxSpanLimit = MIDI_OCTAVE * 2;

int scrollValueForTick(qint64 tick, qint64 duration)
{
    if (duration <= 0)
        return 0;
    if (tick <= 0)
        return 0;
    if (tick >= duration)
        return ScoreScrollRange;
    return static_cast<int>(static_cast<double>(tick) *
                            static_cast<double>(ScoreScrollRange) /
                            static_cast<double>(duration) + 0.5);
}

qint64 tickForScrollValue(int value, qint64 duration)
{
    if (duration <= 0 || value <= 0)
        return 0;
    if (value >= ScoreScrollRange)
        return duration;
    return static_cast<qint64>(static_cast<double>(value) *
                               static_cast<double>(duration) /
                               static_cast<double>(ScoreScrollRange) + 0.5);
}

QString normalizedChordConfigName(const QString& name)
{
    if (name == QLatin1String(ChordConfigBasic) ||
            name == QLatin1String(ChordConfigSevenths) ||
            name == QLatin1String(ChordConfigLeadSheetRepeats))
        return name;
    return QString::fromLatin1(ChordConfigLeadSheet);
}
}

#ifdef __linux__
#ifndef USE_REALTIME_PRIORITY
#define USE_REALTIME_PRIORITY 0
#endif
#endif

#if USE_REALTIME_PRIORITY
#include <sched.h>

/* sets the process to "policy" policy at given priority */
static int set_realtime_priority(int policy, int prio)
{
    struct sched_param schp;
    memset(&schp, 0, sizeof(schp));

    schp.sched_priority = prio;
    if (sched_setscheduler(0, policy, &schp) != 0) {
        perror("sched_setscheduler");
        return -1;
    }
    return 0;
}
#endif

QtWindow::QtWindow()
{
    m_settings = new CSettings(this);
    setWindowIcon(QIcon(":/images/pianobooster.png"));
    setWindowTitle(tr("Piano Booster"));

    Cfg::setDefaults();
    Cfg::setTheme(m_settings->value("View/Theme", PB_THEME_sepiaPaper).toInt());
    Cfg::setViewMode(m_settings->value("View/Mode", PB_VIEW_MODE_score).toInt());
    CNote::setSplitHands(m_settings->value("Song/SplitHands", false).toBool());
    CNote::setSplitHandsMode(static_cast<splitHandsMode_t>(
        m_settings->value("Song/SplitHandsMode", PB_SPLIT_HANDS_naive).toInt()));
    applySplitHandsClusterSpanSettings(
                m_settings->value("Song/SplitHandsClusterNormalMaxSpan",
                                  ClusterNormalMaxSpanDefault).toInt(),
                m_settings->value("Song/SplitHandsClusterWideMaxSpan",
                                  ClusterWideMaxSpanDefault).toInt());

    decodeCommandLine();

    auto fmt = QSurfaceFormat::defaultFormat();
    if (Cfg::experimentalSwapInterval != -1)
    {
        fmt.setSwapInterval(Cfg::experimentalSwapInterval);
        int value = fmt.swapInterval();
        ppLogInfo("Open GL Swap Interval %d", value);
    }

    for (int i = 0; i < maxRecentFiles(); ++i)
         m_recentFileActs[i] = nullptr;
    m_separatorAct = nullptr;

#if USE_REALTIME_PRIORITY
    int rt_prio = sched_get_priority_max(SCHED_FIFO);
    set_realtime_priority(SCHED_FIFO, rt_prio);
#endif

    QString antiAliasingSetting = m_settings->value("anti-aliasing").toString();
    if (antiAliasingSetting.isEmpty() || antiAliasingSetting=="on"){
        fmt.setSamples(4);
    }

    QSurfaceFormat::setDefaultFormat(fmt);

    m_songOwner.reset(new CSong());
    m_scoreOwner.reset(new CScore(m_settings));
    m_song = m_songOwner.get();
    m_score = m_scoreOwner.get();

    m_glWidget = new CGLView(this, m_settings, m_song, m_score);
    m_glWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_controller = new ApplicationController(m_song, m_settings);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    QVBoxLayout *columnLayout = new QVBoxLayout;

    m_sidePanel = new GuiSidePanel(this, m_settings);
    m_topBar = new GuiTopBar(this, m_settings);
    m_scoreScrollBar = new QScrollBar(Qt::Horizontal, this);
    m_scoreScrollBar->setEnabled(false);
    m_scoreScrollBar->setRange(0, 0);
    m_scoreScrollBar->setSingleStep(1);
    m_scoreScrollBar->setPageStep(1);
    m_tutorWindow = new QTextBrowser(this);
    m_tutorWindow->hide();
    m_updatingScoreScrollbar = false;
    m_scoreScrollbarDragging = false;

    m_settings->init(m_song, m_sidePanel, m_topBar);

    mainLayout->addWidget(m_sidePanel);
    columnLayout->addWidget(m_topBar);
    columnLayout->addWidget(m_glWidget);
    columnLayout->addWidget(m_scoreScrollBar);
    columnLayout->addWidget(m_tutorWindow);
    mainLayout->addLayout(columnLayout);

    m_song->init2(m_score, m_settings);

    m_sidePanel->init(m_controller, m_topBar);
    m_topBar->init(m_controller);
    connect(m_scoreScrollBar, SIGNAL(sliderPressed()), this, SLOT(onScoreScrollPressed()));
    connect(m_scoreScrollBar, SIGNAL(sliderMoved(int)), this, SLOT(onScoreScrollMoved(int)));
    connect(m_scoreScrollBar, SIGNAL(sliderReleased()), this, SLOT(onScoreScrollReleased()));
    connect(m_scoreScrollBar, SIGNAL(valueChanged(int)), this, SLOT(onScoreScrollChanged(int)));

    QWidget *centralWin = new QWidget();
    centralWin->setLayout(mainLayout);

    setCentralWidget(centralWin);

    m_glWidget->setFocus(Qt::ActiveWindowFocusReason);

    m_controller->setPianoSoundPatches(
                m_settings->value("Keyboard/RightSound", Cfg::defaultRightPatch()).toInt() - 1,
                m_settings->value("Keyboard/WrongSound", Cfg::defaultWrongPatch()).toInt() - 1,
                true);

    m_controller->setLatencyFix(m_settings->value("Midi/Latency", 0).toInt());

    m_controller->setTimingMarkers(
                m_settings->value("Score/TimingMarkers", m_controller->timingMarkers()).toBool());
    m_controller->setStopPointMode(static_cast<stopPointMode_t>(
                m_settings->value("Score/StopPointMode", m_controller->stopPointMode()).toInt()));
    m_controller->setRhythmTappingMode(static_cast<rhythmTapping_t>(
                m_settings->value("Score/RtyhemTappingMode",
                                  m_controller->rhythmTappingMode()).toInt()));

    m_controller->reconnectMidi();

    readSettings();

    QTimer::singleShot(100, this, [&](){
        QString songName = m_settings->value("CurrentSong").toString();
        if (!songName.isEmpty())
            m_controller->openSongFile(songName);
    });

    init();
}

void QtWindow::init()
{

    m_settings->loadSettings();

    createActions();
    createMenus();
    readSettings();

    refreshTranslate();
    show();
}

void QtWindow::refreshScoreScrollbar()
{
    if (m_scoreScrollBar == nullptr || m_controller == nullptr || m_scoreScrollbarDragging)
        return;

    const ScoreScrollState state = m_controller->scrollbarState();
    const bool enabled = state.durationTicks > 0;
    m_updatingScoreScrollbar = true;
    m_scoreScrollBar->setEnabled(enabled);
    m_scoreScrollBar->setRange(0, enabled ? ScoreScrollRange : 0);
    m_scoreScrollBar->setSingleStep(enabled ? qMax(1, ScoreScrollRange / 400) : 1);
    m_scoreScrollBar->setPageStep(enabled ? qMax(1, ScoreScrollRange / 20) : 1);
    m_scoreScrollBar->setValue(scrollValueForTick(state.currentTick, state.durationTicks));
    m_updatingScoreScrollbar = false;
}

QtWindow::~QtWindow()
{
    if (m_glWidget != nullptr)
        m_glWidget->stopTimerEvent();
    delete m_controller;
    delete m_settings;
}

///////////////////////////////////////////////////////////////////////////////
//! @brief               Displays the usage
void QtWindow::displayUsage()
{
    fprintf(stdout, "Usage: pianobooster [flags] [midifile]\n");
    fprintf(stdout, "  -d, --debug             Increase the debug level.\n");
    fprintf(stdout, "      --Xnote-length      Displays the note length (experimental)\n");
    fprintf(stdout, "  -h, --help              Displays this help message.\n");
    fprintf(stdout, "  -v, --version           Displays version number and then exits.\n");
    fprintf(stdout, "  -l   --log              Write debug info to the \"pb.log\" log file.\n");
    fprintf(stdout, "       --midi-input-dump  Displays the midi input in hex.\n");
    fprintf(stdout, "       --lights           Turns on the keyboard lights.\n");
}

int QtWindow::decodeIntegerParam(const QString &arg, int defaultParam)
{
    int n = arg.lastIndexOf('=');
    if (n == -1 || (n + 1) >= arg.size())
        return defaultParam;
    bool ok;
    int value = arg.mid(n+1).toInt(&ok);
    if (ok)
        return value;
    return defaultParam;
}

bool QtWindow::validateIntegerParam(const QString &arg)
{
    int n = arg.lastIndexOf('=');
    if (n == -1 || (n + 1) >= arg.size())
        return false;
    bool ok;
    arg.mid(n+1).toInt(&ok);
     return ok;
}
bool QtWindow::validateIntegerParamWithMessage(const QString &arg)
{
    bool ok = validateIntegerParam(arg);
    if (!ok) {
        fprintf(stderr, "ERROR: Invalid parameter to a command line argument \"%s\".\n", qPrintable(arg));
        exit(0);
    }
     return ok;
}

void QtWindow::decodeMidiFileArg(const QString &arg)
{

    QFileInfo fileInfo(arg);

    if (!fileInfo.exists() )
    {
        QMessageBox::warning(nullptr, tr("PianoBooster MIDI File Error"),
                 tr("Cannot open \"%1\"").arg(QString(fileInfo.absoluteFilePath())));
        exit(1);
    }
        else if ( !(fileInfo.fileName().endsWith(".mid", Qt::CaseInsensitive ) ||
             fileInfo.fileName().endsWith(".midi", Qt::CaseInsensitive ) ||
             fileInfo.fileName().endsWith(".kar", Qt::CaseInsensitive )) )
    {
        QMessageBox::warning(nullptr, tr("PianoBooster MIDI File Error"),
                 tr("\"%1\" is not a MIDI File").arg(QString(fileInfo.fileName())));
        exit(1);
    }
    else
    {
        bool vaildMidiFile = true;
        QFile file(fileInfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly))
            vaildMidiFile = false;
        else
        {
            QByteArray bytes = file.read(4);
            for (int i = 0; i < 4; i++)
            {
                if (bytes[i] !="MThd"[i] )
                    vaildMidiFile = false;
            }
            file.close();
        }
        if (vaildMidiFile ==  true)
            m_settings->setValue("CurrentSong", fileInfo.absoluteFilePath());
        else
        {
            QMessageBox::warning(nullptr, tr("PianoBooster MIDI File Error"),
                 tr("\"%1\" is not a valid MIDI file").arg(QString(fileInfo.absoluteFilePath())));
            exit(1);
        }
    }
}

void QtWindow::decodeCommandLine()
{
    bool hasMidiFile = false;
    QStringList argList = QCoreApplication::arguments();
    QString arg;
    for (int i = 0; i < argList.size(); ++i)
    {
        arg = argList[i];
        if (arg.startsWith("-"))
        {
            if (arg.startsWith("-d") || arg.startsWith("--debug"))
                Cfg::logLevel++;
            else if (arg.startsWith("--Xnote-length"))
                Cfg::experimentalNoteLength = true;
            else if (arg.startsWith("--Xtick-rate")) {
                if (validateIntegerParamWithMessage(arg)) {
                    Cfg::tickRate = decodeIntegerParam(arg, 12);
                }
            } else if (arg.startsWith("-l") || arg.startsWith("--log"))
                Cfg::useLogFile = true;
            else if (arg.startsWith("--midi-input-dump"))
                Cfg::midiInputDump = true;
            else if (arg.startsWith("-X1"))
                Cfg::experimentalTempo = true;
            else if (arg.startsWith("-Xswap"))
                Cfg::experimentalSwapInterval = decodeIntegerParam(arg, 100);

            else if (arg.startsWith("--lights"))
                Cfg::keyboardLightsChan = 1-1;  // Channel 1 (really a zero)

            else if (arg.startsWith("-h") || arg.startsWith("-?") || arg.startsWith("--help"))
            {
                displayUsage();
                exit(0);
            }
            else if (arg.startsWith("-v") || arg.startsWith("--version"))
            {
                fprintf(stdout, "pianobooster Version " PB_VERSION"\n");
                exit(0);
            }
            else
            {
                fprintf(stderr, "ERROR: Unknown arguments.\n");
                displayUsage();
                exit(0);
            }
        }
        else {
            if ( hasMidiFile == false && i > 0)
            {
                hasMidiFile = true;
                decodeMidiFileArg(arg);
            }
        }
    }
}

void QtWindow::addShortcutAction(const QString & key, const char * method)
{
    QAction* act = new QAction(this);
    act->setShortcut(m_settings->value(key).toString());
    connect(act, SIGNAL(triggered()), this, method);
    addAction(act);
}

void QtWindow::createActions()
{
    m_openAct = new QAction(QIcon(":/images/open.png"), tr("&Open..."), this);
    m_openAct->setShortcut(tr("Ctrl+O"));
    m_openAct->setToolTip(tr("Open an existing file"));
    connect(m_openAct, SIGNAL(triggered()), this, SLOT(open()));

    m_exitAct = new QAction(tr("E&xit"), this);
    m_exitAct->setShortcut(tr("Ctrl+Q"));
    m_exitAct->setToolTip(tr("Exit the application"));
    connect(m_exitAct, SIGNAL(triggered()), this, SLOT(close()));

    m_aboutAct = new QAction(tr("&About"), this);
    m_aboutAct->setToolTip(tr("Show the application's About box"));
    connect(m_aboutAct, SIGNAL(triggered()), this, SLOT(about()));

    m_shortcutAct = new QAction(tr("&PC Shortcut Keys"), this);
    m_shortcutAct->setToolTip(tr("The PC Keyboard shortcut keys"));
    connect(m_shortcutAct, SIGNAL(triggered()), this, SLOT(keyboardShortcuts()));

    m_setupMidiAct = new QAction(tr("&MIDI Setup ..."), this);
    m_setupMidiAct->setShortcut(tr("Ctrl+S"));
    m_setupMidiAct->setToolTip(tr("Setup the MIDI input and output"));
    connect(m_setupMidiAct, SIGNAL(triggered()), this, SLOT(showMidiSetup()));

    m_setupKeyboardAct = new QAction(tr("Piano &Keyboard Setting ..."), this);
    m_setupKeyboardAct->setShortcut(tr("Ctrl+K"));
    m_setupKeyboardAct->setToolTip(tr("Change the piano keyboard settings"));
    connect(m_setupKeyboardAct, SIGNAL(triggered()), this, SLOT(showKeyboardSetup()));

    m_fullScreenStateAct = new QAction(tr("&Fullscreen"), this);
    m_fullScreenStateAct->setToolTip(tr("Fullscreen mode"));
    m_fullScreenStateAct->setShortcut(tr("F11"));
    m_fullScreenStateAct->setCheckable(true);
    connect(m_fullScreenStateAct, SIGNAL(triggered()), this, SLOT(onFullScreenStateAct()));

    m_sidePanelStateAct = new QAction(tr("&Show the Side Panel"), this);
    m_sidePanelStateAct->setToolTip(tr("Show the Left Side Panel"));
    m_sidePanelStateAct->setShortcut(tr("F12"));
    m_sidePanelStateAct->setCheckable(true);
    m_sidePanelStateAct->setChecked(true);
    connect(m_sidePanelStateAct, SIGNAL(triggered()), this, SLOT(toggleSidePanel()));

    m_viewPianoKeyboard = new QAction(tr("Show Piano &Keyboard"), this);
    m_viewPianoKeyboard->setToolTip(tr("Show Piano Keyboard Widget"));
    m_viewPianoKeyboard->setCheckable(true);
    m_viewPianoKeyboard->setChecked(false);
    if (m_settings->value("View/PianoKeyboard").toString()=="on"){
        m_viewPianoKeyboard->setChecked(true);
    }
    connect(m_viewPianoKeyboard, SIGNAL(triggered()), this, SLOT(onViewPianoKeyboard()));

    createViewModeActions();

    m_themeGroup = new QActionGroup(this);
    m_themeGroup->setExclusive(true);
    connect(m_themeGroup, SIGNAL(triggered(QAction*)), this, SLOT(onTheme(QAction*)));

    m_themeSepiaPaperAct = new QAction(tr("&Sepia Paper"), this);
    m_themeSepiaPaperAct->setToolTip(tr("Light sepia paper with dark ink"));
    m_themeSepiaPaperAct->setCheckable(true);
    m_themeSepiaPaperAct->setData(PB_THEME_sepiaPaper);
    m_themeGroup->addAction(m_themeSepiaPaperAct);

    m_themeWhitePaperAct = new QAction(tr("&White Paper"), this);
    m_themeWhitePaperAct->setToolTip(tr("White paper with black notation"));
    m_themeWhitePaperAct->setCheckable(true);
    m_themeWhitePaperAct->setData(PB_THEME_whitePaper);
    m_themeGroup->addAction(m_themeWhitePaperAct);

    m_themeClassicDarkAct = new QAction(tr("&Classic Dark"), this);
    m_themeClassicDarkAct->setToolTip(tr("Original black background with green notation"));
    m_themeClassicDarkAct->setCheckable(true);
    m_themeClassicDarkAct->setData(PB_THEME_classicDark);
    m_themeGroup->addAction(m_themeClassicDarkAct);

    if (Cfg::theme() == PB_THEME_classicDark)
        m_themeClassicDarkAct->setChecked(true);
    else if (Cfg::theme() == PB_THEME_whitePaper)
        m_themeWhitePaperAct->setChecked(true);
    else
        m_themeSepiaPaperAct->setChecked(true);

    m_setupPreferencesAct = new QAction(tr("&Preferences ..."), this);
    m_setupPreferencesAct->setToolTip(tr("Settings"));
    m_setupPreferencesAct->setShortcut(tr("Ctrl+P"));
    connect(m_setupPreferencesAct, SIGNAL(triggered()), this, SLOT(showPreferencesDialog()));

    m_songDetailsAct = new QAction(tr("Song &Details ..."), this);
    m_songDetailsAct->setToolTip(tr("Song Settings"));
    m_songDetailsAct->setShortcut(tr("Ctrl+D"));
    connect(m_songDetailsAct, SIGNAL(triggered()), this, SLOT(showSongDetailsDialog()));

    m_annotateChordsAct = new QAction(tr("Annotate &Chords"), this);
    m_annotateChordsAct->setToolTip(tr("Show lead-sheet chord symbols above the score and in synthesia mode"));
    m_annotateChordsAct->setCheckable(true);
    m_annotateChordsAct->setChecked(m_settings->value("Song/AnnotateScore", false).toBool());
    connect(m_annotateChordsAct, SIGNAL(toggled(bool)), this, SLOT(on_annotateChords(bool)));

    m_annotateChordsConfigGroup = new QActionGroup(this);
    m_annotateChordsConfigGroup->setExclusive(true);
    connect(m_annotateChordsConfigGroup, SIGNAL(triggered(QAction*)),
            this, SLOT(on_annotateChordsConfig(QAction*)));
    addAnnotateChordsConfigAction(tr("&Basic"),
                                  tr("Show simple triads and suspensions"),
                                  QString::fromLatin1(ChordConfigBasic));
    addAnnotateChordsConfigAction(tr("&Sevenths"),
                                  tr("Show triads, sixths, and seventh chords"),
                                  QString::fromLatin1(ChordConfigSevenths));
    addAnnotateChordsConfigAction(tr("&Lead Sheet"),
                                  tr("Show sevenths and strong extensions"),
                                  QString::fromLatin1(ChordConfigLeadSheet));
    addAnnotateChordsConfigAction(tr("Lead Sheet + &Repeats"),
                                  tr("Repeat the previous chord through empty bars"),
                                  QString::fromLatin1(ChordConfigLeadSheetRepeats));
    const QString chordConfig = normalizedChordConfigName(
                m_settings->value(ChordConfigSetting,
                                  QString::fromLatin1(ChordConfigLeadSheet)).toString());
    selectAnnotateChordsConfigAction(chordConfig);
    if (!m_settings->contains(ChordConfigSetting))
        m_settings->setValue(ChordConfigSetting, chordConfig);

    m_annotateChordsMaxSegmentsAct = new QAction(tr("&Max Segments Per Bar..."), this);
    m_annotateChordsMaxSegmentsAct->setToolTip(
                tr("Set 1 for one chord per bar, 2 for up to two chords per bar"));
    connect(m_annotateChordsMaxSegmentsAct, SIGNAL(triggered()),
            this, SLOT(on_annotateChordsMaxSegments()));

    m_splitHandsAct = new QAction(tr("Split &Hands"), this);
    m_splitHandsAct->setToolTip(tr("Split a single piano part into left and right hands without changing the MIDI file"));
    m_splitHandsAct->setCheckable(true);
    m_splitHandsAct->setChecked(CNote::splitHandsEnabled());
    connect(m_splitHandsAct, SIGNAL(toggled(bool)), this, SLOT(on_splitHands(bool)));

    m_splitHandsModeGroup = new QActionGroup(this);
    m_splitHandsModeGroup->setExclusive(true);
    m_splitHandsNaiveAct = new QAction(tr("&Naive"), this);
    m_splitHandsNaiveAct->setToolTip(tr("Split each chord around its pitch gap"));
    m_splitHandsNaiveAct->setCheckable(true);
    m_splitHandsNaiveAct->setData(PB_SPLIT_HANDS_naive);
    m_splitHandsModeGroup->addAction(m_splitHandsNaiveAct);
    connect(m_splitHandsNaiveAct, SIGNAL(triggered()), this, SLOT(on_splitHandsMode()));

    m_splitHandsCostAct = new QAction(tr("&Cost Minimized"), this);
    m_splitHandsCostAct->setToolTip(tr("Use global cost minimization for virtual hand assignment"));
    m_splitHandsCostAct->setCheckable(true);
    m_splitHandsCostAct->setData(PB_SPLIT_HANDS_cost);
    m_splitHandsModeGroup->addAction(m_splitHandsCostAct);
    connect(m_splitHandsCostAct, SIGNAL(triggered()), this, SLOT(on_splitHandsMode()));

    m_splitHandsClusterAct = new QAction(tr("C&lustering Based"), this);
    m_splitHandsClusterAct->setToolTip(tr("Use local pitch clustering for fast virtual hand assignment"));
    m_splitHandsClusterAct->setCheckable(true);
    m_splitHandsClusterAct->setData(PB_SPLIT_HANDS_cluster);
    m_splitHandsModeGroup->addAction(m_splitHandsClusterAct);
    connect(m_splitHandsClusterAct, SIGNAL(triggered()), this, SLOT(on_splitHandsMode()));

    m_splitHandsVoicesAct = new QAction(tr("&Voice Separation"), this);
    m_splitHandsVoicesAct->setToolTip(tr("Separate voices, then group voices into virtual hands"));
    m_splitHandsVoicesAct->setCheckable(true);
    m_splitHandsVoicesAct->setData(PB_SPLIT_HANDS_voices);
    m_splitHandsModeGroup->addAction(m_splitHandsVoicesAct);
    connect(m_splitHandsVoicesAct, SIGNAL(triggered()), this, SLOT(on_splitHandsMode()));

    m_splitHandsCreateChannelsAct = new QAction(tr("Split by &Tracks"), this);
    m_splitHandsCreateChannelsAct->setToolTip(tr("Use existing MIDI tracks to create virtual left and right hand channels"));
    m_splitHandsCreateChannelsAct->setCheckable(true);
    m_splitHandsCreateChannelsAct->setData(PB_SPLIT_HANDS_createChannels);
    m_splitHandsModeGroup->addAction(m_splitHandsCreateChannelsAct);
    connect(m_splitHandsCreateChannelsAct, SIGNAL(triggered()), this, SLOT(on_splitHandsMode()));

    m_splitHandsClusterNormalSpanAct = new QAction(tr("Cluster &Normal Max Span..."), this);
    m_splitHandsClusterNormalSpanAct->setToolTip(
                tr("Set the normal maximum hand span for clustering split hands"));
    connect(m_splitHandsClusterNormalSpanAct, SIGNAL(triggered()),
            this, SLOT(on_splitHandsClusterNormalSpan()));

    m_splitHandsClusterWideSpanAct = new QAction(tr("Cluster Repeated &Wide Max Span..."), this);
    m_splitHandsClusterWideSpanAct->setToolTip(
                tr("Set the maximum hand span allowed for repeated wide clustering patterns"));
    connect(m_splitHandsClusterWideSpanAct, SIGNAL(triggered()),
            this, SLOT(on_splitHandsClusterWideSpan()));

    if (CNote::splitHandsMode() == PB_SPLIT_HANDS_createChannels)
        m_splitHandsCreateChannelsAct->setChecked(true);
    else if (CNote::splitHandsMode() == PB_SPLIT_HANDS_voices)
        m_splitHandsVoicesAct->setChecked(true);
    else if (CNote::splitHandsMode() == PB_SPLIT_HANDS_cluster)
        m_splitHandsClusterAct->setChecked(true);
    else if (CNote::splitHandsMode() == PB_SPLIT_HANDS_cost)
        m_splitHandsCostAct->setChecked(true);
    else
        m_splitHandsNaiveAct->setChecked(true);

    QAction* act = new QAction(this);
    act->setShortcut(tr("Shift+F1"));
    connect(act, SIGNAL(triggered()), this, SLOT(enableFollowTempo()));
    addAction(act);

    act = new QAction(this);
    act->setShortcut(tr("Alt+F1"));
    connect(act, SIGNAL(triggered()), this, SLOT(disableFollowTempo()));
    addAction(act);

    addShortcutAction("ShortCuts/RightHand",        SLOT(on_rightHand()));
    addShortcutAction("ShortCuts/BothHands",        SLOT(on_bothHands()));
    addShortcutAction("ShortCuts/LeftHand",         SLOT(on_leftHand()));
    addShortcutAction("ShortCuts/PlayFromStart",    SLOT(on_playFromStart()));
    addShortcutAction("ShortCuts/PlayPause",        SLOT(on_playPause()));
    addShortcutAction("ShortCuts/Faster",           SLOT(on_faster()));
    addShortcutAction("ShortCuts/Slower",           SLOT(on_slower()));
    addShortcutAction("ShortCuts/NextSong",         SLOT(on_nextSong()));
    addShortcutAction("ShortCuts/PreviousSong",     SLOT(on_previousSong()));
    addShortcutAction("ShortCuts/NextBook",         SLOT(on_nextBook()));
    addShortcutAction("ShortCuts/PreviousBook",     SLOT(on_previousBook()));

     for (int i = 0; i < maxRecentFiles(); ++i) {
         m_recentFileActs[i] = new QAction(this);
         m_recentFileActs[i]->setVisible(false);
         connect(m_recentFileActs[i], SIGNAL(triggered()),
                 this, SLOT(openRecentFile()));
     }
}

void QtWindow::createViewModeActions()
{
    m_viewModeGroup = new QActionGroup(this);
    m_viewModeGroup->setExclusive(true);
    connect(m_viewModeGroup, SIGNAL(triggered(QAction*)), this, SLOT(onViewMode(QAction*)));

    m_scoreModeAct = new QAction(tr("&Score"), this);
    m_scoreModeAct->setToolTip(tr("Show scrolling sheet music"));
    m_scoreModeAct->setCheckable(true);
    m_scoreModeAct->setData(PB_VIEW_MODE_score);
    m_viewModeGroup->addAction(m_scoreModeAct);

    m_synthesiaModeAct = new QAction(tr("S&ynthesia"), this);
    m_synthesiaModeAct->setToolTip(tr("Show falling notes over a piano keyboard"));
    m_synthesiaModeAct->setCheckable(true);
    m_synthesiaModeAct->setData(PB_VIEW_MODE_synthesia);
    m_viewModeGroup->addAction(m_synthesiaModeAct);

    if (Cfg::viewMode() == PB_VIEW_MODE_synthesia)
        m_synthesiaModeAct->setChecked(true);
    else
        m_scoreModeAct->setChecked(true);
}

void QtWindow::createMenus()
{
    m_fileMenu = menuBar()->addMenu(tr("&File"));
    m_fileMenu->setToolTipsVisible(true);
    m_fileMenu->addAction(m_openAct);
    m_separatorAct = m_fileMenu->addSeparator();
    for (int i = 0; i < maxRecentFiles(); ++i)
       m_fileMenu->addAction(m_recentFileActs[i]);
    m_fileMenu->addSeparator();
    m_fileMenu->addAction(m_exitAct);
    updateRecentFileActions();

    m_viewMenu = menuBar()->addMenu(tr("&View"));
    m_viewMenu->setToolTipsVisible(true);
    m_viewMenu->addAction(m_sidePanelStateAct);
    m_viewMenu->addAction(m_fullScreenStateAct);
    m_viewMenu->addAction(m_viewPianoKeyboard);
    addViewModeMenu();
    m_themeMenu = m_viewMenu->addMenu(tr("&Theme"));
    m_themeMenu->addAction(m_themeSepiaPaperAct);
    m_themeMenu->addAction(m_themeWhitePaperAct);
    m_themeMenu->addAction(m_themeClassicDarkAct);

    m_songMenu = menuBar()->addMenu(tr("&Song"));
    m_songMenu->setToolTipsVisible(true);
    m_songMenu->addAction(m_splitHandsAct);
    m_splitHandsConfigMenu = m_songMenu->addMenu(tr("Split-Hand &Configuration"));
    m_splitHandsConfigMenu->addAction(m_splitHandsCreateChannelsAct);
    m_splitHandsConfigMenu->addAction(m_splitHandsNaiveAct);
    m_splitHandsConfigMenu->addAction(m_splitHandsCostAct);
    m_splitHandsConfigMenu->addAction(m_splitHandsClusterAct);
    m_splitHandsConfigMenu->addAction(m_splitHandsVoicesAct);
    m_splitHandsConfigMenu->addSeparator();
    m_splitHandsConfigMenu->addAction(m_splitHandsClusterNormalSpanAct);
    m_splitHandsConfigMenu->addAction(m_splitHandsClusterWideSpanAct);
    m_songMenu->addAction(m_annotateChordsAct);
    m_annotateChordsConfigMenu = m_songMenu->addMenu(tr("Annotate Chords &Configuration"));
    for (QAction *action : m_annotateChordsConfigGroup->actions())
        m_annotateChordsConfigMenu->addAction(action);
    m_annotateChordsConfigMenu->addSeparator();
    m_annotateChordsConfigMenu->addAction(m_annotateChordsMaxSegmentsAct);
    m_songMenu->addSeparator();
    m_songMenu->addAction(m_songDetailsAct);

    m_setupMenu = menuBar()->addMenu(tr("Set&up"));
    m_setupMenu->setToolTipsVisible(true);
    m_setupMenu->addAction(m_setupMidiAct);
    m_setupMenu->addAction(m_setupKeyboardAct);
    m_setupMenu->addAction(m_setupPreferencesAct);

    m_helpMenu = menuBar()->addMenu(tr("&Help"));
    m_helpMenu->setToolTipsVisible(true);

    QAction* act;
    act = new QAction(tr("&Help"), this);
    act->setToolTip(tr("Piano Booster Help"));
    connect(act, SIGNAL(triggered()), this, SLOT(help()));
    m_helpMenu->addAction(act);

    act = new QAction(tr("&Website"), this);
    act->setToolTip(tr("Piano Booster Website"));
    connect(act, SIGNAL(triggered()), this, SLOT(website()));
    m_helpMenu->addAction(act);

    m_helpMenu->addAction(m_shortcutAct);
    m_helpMenu->addAction(m_aboutAct);
}

void QtWindow::addViewModeMenu()
{
    QMenu *modeMenu = m_viewMenu->addMenu(tr("&Mode"));
    modeMenu->addAction(m_scoreModeAct);
    modeMenu->addAction(m_synthesiaModeAct);
}

void QtWindow::applySplitHandsClusterSpanSettings(int normalMaxSpan, int repeatedWideMaxSpan)
{
    CNote::setClusterMaxHandSpans(normalMaxSpan, repeatedWideMaxSpan);
    if (m_settings == nullptr)
        return;
    m_settings->setValue("Song/SplitHandsClusterNormalMaxSpan", CNote::clusterNormalMaxHandSpan());
    m_settings->setValue("Song/SplitHandsClusterWideMaxSpan", CNote::clusterRepeatedWideMaxHandSpan());
}

QAction* QtWindow::addAnnotateChordsConfigAction(const QString &text,
                                                 const QString &toolTip,
                                                 const QString &configName)
{
    QAction *action = new QAction(text, this);
    action->setToolTip(toolTip);
    action->setCheckable(true);
    action->setData(configName);
    m_annotateChordsConfigGroup->addAction(action);
    return action;
}

void QtWindow::selectAnnotateChordsConfigAction(const QString &configName)
{
    const QString normalized = normalizedChordConfigName(configName);
    for (QAction *action : m_annotateChordsConfigGroup->actions())
    {
        if (action->data().toString() == normalized)
        {
            action->setChecked(true);
            return;
        }
    }
}

void QtWindow::applyAnnotateChordsConfig(const QString &configName)
{
    const QString name = normalizedChordConfigName(configName);
    int detail = ChordAnnotationExtensions;
    bool carryEmptyBars = name == QLatin1String(ChordConfigLeadSheetRepeats);
    if (name == QLatin1String(ChordConfigBasic))
        detail = ChordAnnotationBasic;
    else if (name == QLatin1String(ChordConfigSevenths))
        detail = ChordAnnotationSevenths;
    const int maxSegments = qBound(1, m_settings->value("Song/AnnotateMaxSegmentsPerBar", 1).toInt(),
                                   ChordConfigMaxSegmentsLimit);

    m_settings->setValue(ChordConfigSetting, name);
    m_settings->setValue("Song/AnnotateUseSmoothing", true);
    m_settings->setValue("Song/AnnotateCarryEmptyBars", carryEmptyBars);
    m_settings->setValue("Song/AnnotateIntraBarSegmentation", maxSegments > 1);
    m_settings->setValue("Song/AnnotateMaxSegmentsPerBar", maxSegments);
    m_settings->setValue("Song/AnnotateSourceChannel", -1);
    m_settings->setValue("Song/AnnotateSourceTrack", -1);
    m_settings->setValue("Song/AnnotateDetail", detail);
    m_settings->setValue("Song/AnnotateLowConfidenceMode", ChordAnnotationHideLowConfidence);
    m_settings->setValue("Song/AnnotateMinConfidence", 0.08);
}

void QtWindow::openRecentFile()
{
    QAction *action = qobject_cast<QAction *>(sender());
     if (action)
         m_controller->openSongFile(action->data().toString());
}

void QtWindow::showMidiSetup(){

    m_topBar->stopMuiscPlaying();

    m_glWidget->stopTimerEvent();
    GuiMidiSetupDialog midiSetupDialog(this);
    midiSetupDialog.init(m_controller, m_settings);
    midiSetupDialog.exec();
    m_controller->flushMidiInput();
    m_glWidget->startTimerEvent();
}

void QtWindow::on_splitHands(bool checked)
{
    CNote::setSplitHands(checked);
    m_settings->setValue("Song/SplitHands", checked);

    const QString songFile = m_settings->getCurrentSongLongFileName();
    if (songFile.isEmpty() || !QFile::exists(songFile))
        return;

    if (m_controller->playing())
    {
        m_controller->pause();
        m_topBar->setPlayButtonState(false);
    }

    m_controller->rewind();
    m_sidePanel->refresh();
    m_controller->forceScoreRedraw();
}

void QtWindow::on_splitHandsMode()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action)
        return;

    CNote::setSplitHandsMode(static_cast<splitHandsMode_t>(action->data().toInt()));
    m_settings->setValue("Song/SplitHandsMode", CNote::splitHandsMode());
    if (CNote::splitHandsCreateChannels() && !CNote::splitHandsEnabled())
    {
        CNote::setSplitHands(true);
        m_settings->setValue("Song/SplitHands", true);
        bool wasBlocked = m_splitHandsAct->blockSignals(true);
        m_splitHandsAct->setChecked(true);
        m_splitHandsAct->blockSignals(wasBlocked);
    }

    const QString songFile = m_settings->getCurrentSongLongFileName();
    if (songFile.isEmpty() || !QFile::exists(songFile))
        return;

    if (m_controller->playing())
    {
        m_controller->pause();
        m_topBar->setPlayButtonState(false);
    }

    m_controller->rewind();
    m_sidePanel->refresh();
    m_controller->forceScoreRedraw();
}

void QtWindow::on_splitHandsClusterNormalSpan()
{
    bool accepted = false;
    const int current = CNote::clusterNormalMaxHandSpan();
    const int value = QInputDialog::getInt(this, tr("Cluster Normal Max Span"),
                                           tr("Normal max hand span, in semitones:"),
                                           current, 1, ClusterMaxSpanLimit, 1, &accepted);
    if (!accepted)
        return;
    applySplitHandsClusterSpanSettings(value, CNote::clusterRepeatedWideMaxHandSpan());
    m_controller->rebuildScoreData();
    m_sidePanel->refresh();
    m_glWidget->update();
}

void QtWindow::on_splitHandsClusterWideSpan()
{
    bool accepted = false;
    const int normal = CNote::clusterNormalMaxHandSpan();
    const int current = qMax(normal, CNote::clusterRepeatedWideMaxHandSpan());
    const int value = QInputDialog::getInt(this, tr("Cluster Repeated Wide Max Span"),
                                           tr("Repeated wide-pattern max hand span, in semitones:"),
                                           current, normal, ClusterMaxSpanLimit, 1, &accepted);
    if (!accepted)
        return;
    applySplitHandsClusterSpanSettings(normal, value);
    m_controller->rebuildScoreData();
    m_sidePanel->refresh();
    m_glWidget->update();
}

void QtWindow::on_annotateChords(bool checked)
{
    m_settings->setValue("Song/AnnotateScore", checked);
    m_controller->invalidateScoreRendererCaches();
    m_glWidget->update();
}

void QtWindow::on_annotateChordsConfig(QAction *action)
{
    if (action == nullptr)
        return;
    applyAnnotateChordsConfig(action->data().toString());
    m_controller->rebuildScoreData();
    m_glWidget->update();
}

void QtWindow::on_annotateChordsMaxSegments()
{
    bool accepted = false;
    const int current = qBound(1, m_settings->value("Song/AnnotateMaxSegmentsPerBar", 1).toInt(),
                               ChordConfigMaxSegmentsLimit);
    const int maxSegments = QInputDialog::getInt(this, tr("Max Segments Per Bar"),
                                                 tr("Max chord segments per bar:"), current, 1,
                                                 ChordConfigMaxSegmentsLimit, 1, &accepted);
    if (!accepted)
        return;
    m_settings->setValue("Song/AnnotateMaxSegmentsPerBar", maxSegments);
    m_settings->setValue("Song/AnnotateIntraBarSegmentation", maxSegments > 1);
    m_controller->rebuildScoreData();
    m_glWidget->update();
}

void QtWindow::onTheme(QAction *action)
{
    if (!action)
        return;

    Cfg::setTheme(action->data().toInt());
    m_settings->setValue("View/Theme", Cfg::theme());
    m_controller->invalidateScoreRendererCaches();
    m_glWidget->update();
}

void QtWindow::onViewMode(QAction *action)
{
    if (!action)
        return;

    Cfg::setViewMode(action->data().toInt());
    m_settings->setValue("View/Mode", Cfg::viewMode());
    m_controller->invalidateScoreRendererCaches();
    m_glWidget->update();
}

void QtWindow::seekWithScoreScrollbar(int value)
{
    const ScoreScrollState state = m_controller->scrollbarState();
    if (state.durationTicks <= 0)
        return;

    const qint64 tick = tickForScrollValue(value, state.durationTicks);
    m_controller->seekTick(tick);
    m_controller->forceScoreRedraw();
    m_topBar->setPlayButtonState(m_controller->playing(), false);
    m_glWidget->update();
}

void QtWindow::onScoreScrollPressed()
{
    m_scoreScrollbarDragging = true;
    m_controller->beginScrollbarDrag();
}

void QtWindow::onScoreScrollMoved(int value)
{
    const ScoreScrollState state = m_controller->scrollbarState();
    const qint64 tick = tickForScrollValue(value, state.durationTicks);
    m_controller->updateScrollbarDrag(tick);
    m_controller->forceScoreRedraw();
    m_glWidget->update();
}

void QtWindow::onScoreScrollReleased()
{
    const ScoreScrollState state = m_controller->scrollbarState();
    const qint64 tick = tickForScrollValue(m_scoreScrollBar->value(), state.durationTicks);
    m_controller->finishScrollbarDrag(tick);
    m_topBar->setPlayButtonState(m_controller->playing(), false);
    m_scoreScrollbarDragging = false;
    refreshScoreScrollbar();
    m_glWidget->update();
}

void QtWindow::onScoreScrollChanged(int value)
{
    if (m_updatingScoreScrollbar || m_scoreScrollbarDragging)
        return;
    seekWithScoreScrollbar(value);
    refreshScoreScrollbar();
}

// load the recent file list from the config file into the file menu
void QtWindow::updateRecentFileActions()
{

    QStringList files = m_settings->value("RecentFileList").toStringList();

    int numRecentFiles = qMin(files.size(), maxRecentFiles());

    for (int i = 0; i < numRecentFiles; ++i) {
        QString text = tr("&%1 %2").arg(i + 1).arg(strippedName(files[i]));
        if (m_recentFileActs[i] == nullptr)
            break;
        m_recentFileActs[i]->setText(text);
        m_recentFileActs[i]->setData(files[i]);
        m_recentFileActs[i]->setVisible(true);
    }

    for (int j = numRecentFiles; j < maxRecentFiles(); ++j) {
        if (m_recentFileActs[j] == nullptr)
            break;
        m_recentFileActs[j]->setVisible(false);
    }

    if (m_separatorAct)
        m_separatorAct->setVisible(numRecentFiles > 0);
}

QString QtWindow::strippedName(const QString &fullFileName)
{
    return QFileInfo(fullFileName).fileName();
}

// Just used for the RecentFileList
void QtWindow::setCurrentFile(const QString &fileName)
{

    setWindowFilePath(fileName);

    QStringList files = m_settings->value("RecentFileList").toStringList();
    files.removeAll(fileName);
    files.prepend(fileName);
    while (files.size() > maxRecentFiles())
        files.removeLast();

    m_settings->setValue("RecentFileList", files);

    updateRecentFileActions();

}

void QtWindow::website()
{
    QDesktopServices::openUrl(QUrl("https://www.pianobooster.org/"));
}

void QtWindow::help()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle (tr("Piano Booster Help"));
    msgBox.setText(
    tr("<h3>Getting Started</h3>") %
    tr("<p>You need a <b>MIDI Piano Keyboard </b> and a <b>MIDI interface</b> for the PC. If you "
       "don't have a MIDI keyboard you can still try out PianoBooster using the PC keyboard, 'X' is "
       "middle C.</p>") %

    tr("<p>PianoBooster now includes a built-in sound generator called FluidSynth "
    "which requires a General MIDI (GM) SoundFont. "
    "Use the 'Setup/MIDI Setup' menu option and then the load button on the FluidSynth tab to install the SoundFont.</p>")   %

    tr("<p>PianoBooster works best with MIDI files that have separate left and right piano parts "
       "using MIDI channels 3 and 4.") %
    tr("<h3>Setting Up</h3>") %
    tr("<p>First use the <i>Setup/MIDI Setup</i> menu and in the dialog box select the MIDI input and MIDI "
       "output interfaces that match your hardware. ") %
    tr("Next use <i>File/Open</i> to open the MIDI file \".mid\" or a karaoke \".kar\" file. "
       "Now select whether you want to just <i>listen</i> to the music or "
       "<i>play along</i> on the piano keyboard by setting the <i>skill</i> level on the side panel. Finally when "
       "you are ready click the <i>play icon</i> (or press the <i>space bar</i>) to roll the music.") %
    tr("<h3>Hints on Playing the Piano</h3>"
       "<p>For hints on how to play the piano see: ") %
       "<a href=\"https://www.pianobooster.org/music-info.html\" ><b>" % tr("Piano Hints") % QStringLiteral("</b></a></p>") %
    tr("<h3>More Information</h3>"
       "<p>For more help please visit the PianoBooster ") %
       "<a href=\"https://www.pianobooster.org\" ><b>" + tr("website") + "</b></a>, " %
    tr("the PianoBooster") + " <a href=\"https://www.pianobooster.org/faq.html\" ><b> " + tr("FAQ") + QStringLiteral("</b></a> ") %
    tr("and the") % QStringLiteral(" <a href=\"http://piano-booster.2625608.n2.nabble.com/Piano-Booster-Users-f1591936.html\"><b>") % tr("user forum") % QStringLiteral("</b></a>.")
    );

    msgBox.setMinimumWidth(600);
    msgBox.exec();
}

void QtWindow::about()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle (tr("About Piano Booster"));
    msgBox.setText(
            tr("<b>PianoBooster - Version %1</b> <br><br>").arg(PB_VERSION) %
            tr("<b>Boost</b> your <b>Piano</b> playing skills!<br><br>") %
            QStringLiteral("<a href=\"https://www.pianobooster.org/\" ><b>https://www.pianobooster.org/</b></a><br><br>") %
            tr("Copyright(c) L. J. Barman, 2008-2020; All rights reserved.<br>") %
            tr("Copyright(c) Fabien Givors, 2018-2019; All rights reserved.<br>") %
            tr("Copyright(c) Marius Kittler, 2021-2022; All rights reserved.<br>") %
            QStringLiteral("<br>") %
            tr("This program is made available "
                "under the terms of the GNU General Public License version 3 as published by "
                "the Free Software Foundation.<br><br>"
            )
            #ifdef USE_BUNDLED_RTMIDI
             %
            tr("This program also contains RtMIDI: realtime MIDI i/o C++ classes<br>") %
            tr("Copyright(c) Gary P. Scavone, 2003-2019; All rights reserved.")
            #endif
    );
    msgBox.setMinimumWidth(600);
    msgBox.exec();
}

QString QtWindow::displayShortCut(const QString &key, const QString &description)
{
    QString str = QStringLiteral("<tr>"
                "<td>%1</td>"
                "<td>%2</td>"
                "</tr>").arg( description, tr(m_settings->value(key).toString().toUtf8().data()));
    return str;

}

void QtWindow::keyboardShortcuts()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle (tr("PC Keyboard ShortCuts"));
    QString msg =
            tr(
                "<h2><center>Keyboard shortcuts</center></h2>"
                "<p>The following PC keyboard shortcuts have been defined.</p>"
                "<center><table  border='1' cellspacing='0' cellpadding='4' >"
                );

    msg += tr(
                "<tr>"
                "<th>Action</th>"
                "<th>Key</th>"
                "</tr>"
            );
    msg += displayShortCut("ShortCuts/RightHand", tr("Choose the right hand"));
    msg += displayShortCut("ShortCuts/BothHands", tr("Choose both hands"));
    msg += displayShortCut("ShortCuts/LeftHand", tr("Choose the left Hand"));
    msg += displayShortCut("ShortCuts/PlayFromStart", tr("Play from start toggle"));
    msg += displayShortCut("ShortCuts/PlayPause", tr("Play Pause Toggle"));
    msg += displayShortCut("ShortCuts/Faster",  tr("Increase the speed by 5%"));
    msg += displayShortCut("ShortCuts/Slower", tr("Increase the speed by 5%"));
    msg += displayShortCut("ShortCuts/NextSong", tr("Change to the Next Song"));
    msg += displayShortCut("ShortCuts/PreviousSong", tr("Change to the Previous Song"));
    msg += displayShortCut("ShortCuts/NextBook", tr("Change to the Next Book"));
    msg += displayShortCut("ShortCuts/PreviousBook", tr("Change to the Previous Book"));

    msg += tr(
                "<tr><td>Fake Piano keys</td><td>X is middle C</td></tr>"
                "</table> </center><br>"
                );
    msgBox.setText(msg);

    msgBox.setMinimumWidth(600);
    msgBox.exec();
}

void QtWindow::open()
{
    m_glWidget->stopTimerEvent();

    const auto currentSong = QFileInfo(m_settings->getCurrentSongLongFileName());
    const auto dir = currentSong.isFile() ? currentSong.path() : QDir::homePath();
    const auto fileName = QFileDialog::getOpenFileName(this,tr("Open MIDI File"),
                            dir, tr("MIDI Files") + " (*.mid *.MID *.midi *.MIDI *.kar *.KAR)");
    if (!fileName.isEmpty()) {
        m_controller->openSongFile(fileName);
        setCurrentFile(fileName);
    }
    m_controller->flushMidiInput();
    m_glWidget->startTimerEvent();
}

void QtWindow::readSettings()
{
    QPoint pos = m_settings->value("Window/Pos", QPoint(25, 25)).toPoint();
    QSize size = m_settings->value("Window/Size", QSize(1200, 800)).toSize();
    resize(size);
    move(pos);
}

void QtWindow::writeSettings()
{
    m_settings->setValue("Window/Pos", pos());
    m_settings->setValue("Window/Size", size());
    m_settings->writeSettings();
}

void QtWindow::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event)
    if (m_controller->playing())
        m_controller->pause();

    writeSettings();
}

void QtWindow::keyPressEvent ( QKeyEvent * event )
{
    if (event->text().length() == 0)
        return;

    if (event->isAutoRepeat() == true)
        return;

    if (event->key() == Qt::Key_F1)
        return;

    int c = event->text().toLatin1().at(0);
    m_controller->pcKeyPress(c, true);
}

void QtWindow::keyReleaseEvent ( QKeyEvent * event )
{
    if (event->isAutoRepeat() == true)
        return;

    if (event->text().length() == 0)
        return;

    int c = event->text().toLatin1().at(0);
    m_controller->pcKeyPress(c, false);
}

void QtWindow::loadTutorHtml(const QString & name)
{
    if (name.isEmpty())
    {
        m_tutorWindow->hide();
        m_tutorWindow->clear();
    }
    else
    {
        QFile file(name);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

        QTextStream out(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        out.setEncoding(QStringConverter::Utf8);
#else
        out.setCodec("UTF-8");
#endif

        QString htmlStart = "<head><style> body{background-color:#FFFFC0;color: black} p{font-size: 18px;} blockquote{color: #ff0000;}</style></head><body>";
        QString htmlBody = out.readAll();
        QString htmlEnd = "</body>";
        QString htmlText = htmlStart + htmlBody + htmlEnd;
        m_tutorWindow->setHtml(htmlText.toUtf8().data());

        // TODO get this working again on small screens
        //_tutorWindow->setFixedHeight(130);
        m_tutorWindow->setFixedHeight(180);

        m_tutorWindow->show();

        file.close();
    }

}

void QtWindow::refreshTranslate(){
#ifndef NO_LANGS
    QString locale = m_settings->selectedLangauge();

    qApp->removeTranslator(&translator);
    qApp->removeTranslator(&translatorMusic);
    qApp->removeTranslator(&qtTranslator);

    // save original
    if (listWidgetsRetranslateUi.size()==0){
        QList<QWidget*> l2 = this->findChildren<QWidget *>();
        for (auto &w:l2){
            QMap<QString,QString> m;
            m["toolTip"]=w->toolTip();
            m["whatsThis"]=w->whatsThis();
            m["windowTitle"]=w->windowTitle();
            m["statusTip"]=w->statusTip();
            listWidgetsRetranslateUi[w]=m;
        }

        QList<QAction*> l = this->findChildren<QAction *>();
        for (auto &w:l){
            QMap<QString,QString> m;
            m["toolTip"]=w->toolTip();
            m["whatsThis"]=w->whatsThis();
            m["statusTip"]=w->statusTip();
            m["text"]=w->text();
            listActionsRetranslateUi[w]=m;
        }
    }

    QString translationsDir = QApplication::applicationDirPath() + "/translations/";

    QFile fileTestLocale(translationsDir);
    if (!fileTestLocale.exists()){
 #if defined (Q_OS_LINUX) || defined (Q_OS_UNIX)
        translationsDir=Util::dataDir()+"/translations/";
 #endif
 #ifdef Q_OS_DARWIN
        translationsDir=QApplication::applicationDirPath() + "/../Resources/translations/";
 #endif
    }
    ppLogInfo("Translations loaded from '%s'",  qPrintable(translationsDir));

    // set translator for app
    auto ok = true;
    if (!translator.load(QSTR_APPNAME + QString("_") + locale , translationsDir))
        ok = ok & translator.load(QSTR_APPNAME + QString("_") + locale, QApplication::applicationDirPath());
    qApp->installTranslator(&translator);

    // set translator for music
    if (!translatorMusic.load(QString("music_") + locale , translationsDir))
       if (!translatorMusic.load(QString("music_") + locale, QApplication::applicationDirPath()  + "/translations/"))
           ok = ok & translatorMusic.load(QString("music_") + locale, QApplication::applicationDirPath());
    qApp->installTranslator(&translatorMusic);

    // set translator for default widget's text (for example: QMessageBox's buttons)
#ifdef __WIN32
    ok = ok & qtTranslator.load("qt_"+locale, translationsDir);
#elif QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    ok = ok & qtTranslator.load("qt_"+locale, QLibraryInfo::path(QLibraryInfo::TranslationsPath));
#else
    ok = ok & qtTranslator.load("qt_"+locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath));
#endif
    qApp->installTranslator(&qtTranslator);

    if (!ok) {
        qDebug() << "Unable to load all translations";
    }

    // retranslate UI
    QList<QWidget*> l2 = this->findChildren<QWidget *>();
    for (auto &w:l2){
        if (!w->toolTip().isEmpty()) w->setToolTip(tr(listWidgetsRetranslateUi[w]["toolTip"].toStdString().c_str()));
        if (!w->whatsThis().isEmpty()) w->setWhatsThis(tr(listWidgetsRetranslateUi[w]["whatsThis"].toStdString().c_str()));
        if (!w->windowTitle().isEmpty()) w->setWindowTitle(tr(listWidgetsRetranslateUi[w]["windowTitle"].toStdString().c_str()));
        if (!w->statusTip().isEmpty()) w->setStatusTip(tr(listWidgetsRetranslateUi[w]["statusTip"].toStdString().c_str()));
    }

    QList<QAction*> l = this->findChildren<QAction *>();
    for (auto &w:l){
        if (!w->toolTip().isEmpty()) w->setToolTip(tr(listActionsRetranslateUi[w]["toolTip"].toStdString().c_str()));
        if (!w->whatsThis().isEmpty()) w->setWhatsThis(tr(listActionsRetranslateUi[w]["whatsThis"].toStdString().c_str()));
        if (!w->statusTip().isEmpty()) w->setStatusTip(tr(listActionsRetranslateUi[w]["statusTip"].toStdString().c_str()));
        if (!w->text().isEmpty()) w->setText(tr(listActionsRetranslateUi[w]["text"].toStdString().c_str()));
    }

    m_sidePanel->updateTranslate();
    m_topBar->updateTranslate();
    m_settings->updateWarningMessages();
    m_settings->updateTutorPage();

#endif
}
