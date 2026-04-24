/*********************************************************************************/
/*!
@file           Cfg.h

@brief          Contains all the configuration Information.

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

#ifndef __CFG_H__
#define __CFG_H__

#define OPTION_BENCHMARK_TEST     0
#if OPTION_BENCHMARK_TEST
#define BENCHMARK_INIT()        benchMarkInit()
#define BENCHMARK(id,mesg)      benchMark(id,mesg)
#define BENCHMARK_RESULTS()     benchMarkResults()
#else
#define BENCHMARK_INIT()
#define BENCHMARK(id,mesg)
#define BENCHMARK_RESULTS()
#endif

#define LOG_LEVEL_NONE  0
#define LOG_LEVEL_INFO  1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_DEBUG 3

class CColor
{
public:
    CColor() { red = green = blue = 0; }

    CColor(double r, double g, double b)
    {
        red = static_cast<float>(r);
        green = static_cast<float>(g);
        blue = static_cast<float>(b);
    }
    float red, green, blue;

    bool operator==(CColor color) const
    {
        if (red == color.red && green == color.green && blue == color.blue)
            return true;
        return false;
    }
};

typedef enum {
    PB_THEME_sepiaPaper,
    PB_THEME_whitePaper,
    PB_THEME_classicDark
} theme_t;

typedef enum {
    PB_VIEW_MODE_score,
    PB_VIEW_MODE_synthesia
} viewMode_t;

/*!
 * @brief   Contains all the configuration Information.
 */
class Cfg
{
public:
    static float staveStartX()         {return 32;}
    static float staveEndX()           {return m_staveEndX;}
    static float playZoneX()           {return scrollStartX() + ( staveEndX() - scrollStartX())* 0.36f;}
    static float clefX()               {return staveStartX() + 24;}
    static float timeSignatureX()      {return clefX() + 32;}
    static float keySignatureX()       {return timeSignatureX() + 34;}
    static float scrollStartX()        {return keySignatureX() + 72;}
    static float pianoX()              {return staveStartX();}

    static float staveThickness()      {return 0.85f;}

    static int playZoneEarly()     {return m_playZoneEarly;}
    static int playZoneLate()      {return m_playZoneLate;}
    static int silenceTimeOut()    {return 8000;} // the time in msec before everything goes quiet
    static int chordNoteGap()      {return 10;} // all notes in a cord must be spaced less than this a gap
    static int chordMaxLength()    {return 20;} // the max time between the start and end of a cord

    static void setTheme(int theme)
    {
        if (theme < PB_THEME_sepiaPaper || theme > PB_THEME_classicDark)
            theme = PB_THEME_sepiaPaper;
        m_theme = theme;
    }
    static int theme() {return m_theme;}
    static void setViewMode(int mode)
    {
        if (mode < PB_VIEW_MODE_score || mode > PB_VIEW_MODE_synthesia)
            mode = PB_VIEW_MODE_score;
        m_viewMode = mode;
    }
    static int viewMode() {return m_viewMode;}

    static CColor menuColor()        {return CColor(0.1, 0.6, 0.6);}
    static CColor menuSelectedColor(){return CColor(0.7, 0.7, 0.1);}

    static CColor staveColor()           {return themeColor(0);}
    static CColor staveColorDim()        {return themeColor(1);}
    static CColor noteColor()            {return themeColor(2);}
    static CColor noteColorDim()         {return themeColor(3);}
    static CColor playedGoodColor()      {return themeColor(4);}
    static CColor playedBadColor()       {return themeColor(5);}
    static CColor playedStoppedColor()   {return themeColor(6);}
    static CColor backgroundColor()      {return themeColor(7);}
    static CColor barMarkerColor()       {return themeColor(8);}
    static CColor beatMarkerColor()      {return themeColor(9);}
    static CColor pianoGoodColor()       {return playedGoodColor();}
    static CColor pianoBadColor()        {return themeColor(10);}
    static CColor noteNameColor()        {return themeColor(11);}
    static CColor textColor()            {return themeColor(12);}
    static CColor warningTextColor()     {return themeColor(13);}
    static CColor accuracyBorderColor()  {return themeColor(14);}
    static CColor playZoneFillColor()    {return themeColor(15);}
    static CColor playZoneCenterColor()  {return themeColor(16);}
    static CColor playZoneEdgeColor()    {return themeColor(17);}
    static CColor appBackgroundColor()   {return themeColor(18);}
    static CColor paperEdgeColor()       {return themeColor(19);}
    static CColor paperShadowColor()     {return themeColor(20);}
    static CColor quietTextColor()       {return themeColor(21);}
    static CColor pianoWhiteKeyColor()   {return themeColor(22);}
    static CColor pianoBlackKeyColor()   {return themeColor(23);}
    static CColor pianoKeyEdgeColor()    {return themeColor(24);}
    static CColor synthesiaRightColor()  {return themeColor(25);}
    static CColor synthesiaLeftColor()   {return themeColor(26);}
    static CColor synthesiaGridColor()   {return themeColor(27);}
    static CColor currentThemeColor(CColor color);

    static void setDefaults() {
    #ifdef _WIN32
         tickRate = 12;
    #else
          tickRate = 4; // was 12
    #endif
    }

    static void setStaveEndX(float x)
    {
         m_staveEndX = x;
    }
    static int getAppX(){return m_appX;}
    static int getAppY(){return m_appY;}
    static int getAppWidth(){return m_appWidth;}
    static int getAppHeight(){return m_appHeight;}
    static void setAppDimentions(int x, int y,int width, int height)
    {
        m_appX = x;
        m_appY = y;
        m_appWidth = width;
        m_appHeight = height;
    }

    static int defaultWrongPatch() {return 7;} // Starts at 1
    static int defaultRightPatch() {return 1;} // Starts at 1

    static int logLevel;
    static bool experimentalTempo;
    static bool experimentalNoteLength;
    static int experimentalSwapInterval;
    static int tickRate;
    static bool useLogFile;
    static bool midiInputDump;
    static int keyboardLightsChan;

private:
    static CColor themeColor(int role);
    static bool matchesThemeColor(int role, CColor color);
    static int m_theme;
    static int m_viewMode;
    static float m_staveEndX;
    static int m_appX, m_appY, m_appWidth, m_appHeight;
    static const int m_playZoneEarly;
    static const int m_playZoneLate;
};

#endif //__CFG_H__
