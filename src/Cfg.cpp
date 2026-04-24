/*********************************************************************************/
/*!
@file           Cfg.cpp

@brief          xxx.

@author         L. J. Barman

    Copyright (c)   2008-2013, L. J. Barman, all rights reserved

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

#include "Cfg.h"

namespace {
const int ThemeColorCount = 18;

const CColor sepiaPaper[ThemeColorCount] = {
    CColor(0.13, 0.12, 0.10), CColor(0.55, 0.51, 0.43),
    CColor(0.06, 0.06, 0.05), CColor(0.48, 0.45, 0.38),
    CColor(0.15, 0.38, 0.58), CColor(0.70, 0.23, 0.18),
    CColor(0.78, 0.48, 0.10), CColor(0.95, 0.92, 0.84),
    CColor(0.72, 0.58, 0.35), CColor(0.84, 0.77, 0.63),
    CColor(0.85, 0.08, 0.06), CColor(0.15, 0.15, 0.13),
    CColor(0.10, 0.10, 0.09), CColor(0.70, 0.18, 0.08),
    CColor(0.32, 0.30, 0.26), CColor(0.83, 0.88, 0.87),
    CColor(0.35, 0.48, 0.50), CColor(0.56, 0.65, 0.64)
};

const CColor whitePaper[ThemeColorCount] = {
    CColor(0.08, 0.08, 0.07), CColor(0.52, 0.52, 0.48),
    CColor(0.02, 0.02, 0.02), CColor(0.45, 0.45, 0.42),
    CColor(0.10, 0.33, 0.60), CColor(0.72, 0.20, 0.14),
    CColor(0.75, 0.45, 0.05), CColor(0.99, 0.98, 0.95),
    CColor(0.66, 0.56, 0.40), CColor(0.84, 0.80, 0.70),
    CColor(0.85, 0.08, 0.06), CColor(0.12, 0.12, 0.11),
    CColor(0.08, 0.08, 0.07), CColor(0.75, 0.12, 0.05),
    CColor(0.25, 0.25, 0.23), CColor(0.88, 0.93, 0.96),
    CColor(0.30, 0.44, 0.58), CColor(0.52, 0.62, 0.70)
};

const CColor classicDark[ThemeColorCount] = {
    CColor(0.10, 0.70, 0.10), CColor(0.15, 0.40, 0.15),
    CColor(0.10, 0.90, 0.10), CColor(0.25, 0.45, 0.25),
    CColor(0.50, 0.60, 1.00), CColor(0.80, 0.30, 0.80),
    CColor(1.00, 0.80, 0.00), CColor(0.00, 0.00, 0.00),
    CColor(0.30, 0.25, 0.25), CColor(0.25, 0.20, 0.20),
    CColor(1.00, 0.00, 0.00), CColor(1.00, 1.00, 1.00),
    CColor(1.00, 1.00, 1.00), CColor(1.00, 0.20, 0.00),
    CColor(1.00, 1.00, 1.00), CColor(0.00, 0.00, 0.30),
    CColor(0.00, 0.00, 0.80), CColor(0.00, 0.00, 0.60)
};
}

float Cfg::m_staveEndX;
int Cfg::logLevel = LOG_LEVEL_INFO;
int Cfg::m_appX;
int Cfg::m_appY;
int Cfg::m_appWidth;
int Cfg::m_appHeight;
bool Cfg::experimentalTempo = false;
bool Cfg::experimentalNoteLength = false;
bool Cfg::useLogFile = false;
bool Cfg::midiInputDump = false;
int Cfg::keyboardLightsChan = -1;

int Cfg::experimentalSwapInterval = -1;
int Cfg::tickRate;
int Cfg::m_theme = PB_THEME_sepiaPaper;

const int Cfg::m_playZoneEarly = 25; // Was 25
const int Cfg::m_playZoneLate = 25;

CColor Cfg::themeColor(int role)
{
    if (role < 0 || role >= ThemeColorCount)
        role = 0;
    if (m_theme == PB_THEME_classicDark)
        return classicDark[role];
    if (m_theme == PB_THEME_whitePaper)
        return whitePaper[role];
    return sepiaPaper[role];
}

bool Cfg::matchesThemeColor(int role, CColor color)
{
    return sepiaPaper[role] == color ||
            whitePaper[role] == color ||
            classicDark[role] == color;
}

CColor Cfg::currentThemeColor(CColor color)
{
    for (int role = 0; role < ThemeColorCount; role++)
        if (matchesThemeColor(role, color))
            return themeColor(role);
    return color;
}
