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
const int ThemeColorCount = 28;

const CColor sepiaPaper[ThemeColorCount] = {
    CColor(0.18, 0.16, 0.13), CColor(0.64, 0.58, 0.48),
    CColor(0.07, 0.07, 0.06), CColor(0.50, 0.46, 0.38),
    CColor(0.10, 0.36, 0.64), CColor(0.73, 0.20, 0.16),
    CColor(0.78, 0.49, 0.12), CColor(0.96, 0.93, 0.86),
    CColor(0.60, 0.50, 0.36), CColor(0.78, 0.70, 0.57),
    CColor(0.80, 0.10, 0.08), CColor(0.17, 0.16, 0.14),
    CColor(0.11, 0.10, 0.09), CColor(0.74, 0.18, 0.08),
    CColor(0.36, 0.32, 0.27), CColor(0.73, 0.84, 0.88),
    CColor(0.18, 0.46, 0.61), CColor(0.55, 0.70, 0.77),
    CColor(0.78, 0.75, 0.68), CColor(0.78, 0.72, 0.61),
    CColor(0.49, 0.43, 0.34), CColor(0.42, 0.38, 0.31),
    CColor(0.99, 0.98, 0.94), CColor(0.07, 0.07, 0.07),
    CColor(0.70, 0.67, 0.60), CColor(0.12, 0.48, 0.86),
    CColor(0.90, 0.42, 0.14), CColor(0.66, 0.61, 0.52)
};

const CColor whitePaper[ThemeColorCount] = {
    CColor(0.12, 0.12, 0.11), CColor(0.60, 0.60, 0.55),
    CColor(0.03, 0.03, 0.03), CColor(0.50, 0.50, 0.46),
    CColor(0.07, 0.32, 0.64), CColor(0.72, 0.18, 0.13),
    CColor(0.78, 0.48, 0.08), CColor(0.99, 0.985, 0.96),
    CColor(0.58, 0.50, 0.36), CColor(0.78, 0.74, 0.64),
    CColor(0.82, 0.08, 0.06), CColor(0.14, 0.14, 0.13),
    CColor(0.08, 0.08, 0.07), CColor(0.76, 0.12, 0.05),
    CColor(0.30, 0.30, 0.27), CColor(0.74, 0.86, 0.92),
    CColor(0.12, 0.42, 0.62), CColor(0.48, 0.66, 0.76),
    CColor(0.88, 0.89, 0.86), CColor(0.80, 0.79, 0.73),
    CColor(0.54, 0.54, 0.50), CColor(0.44, 0.44, 0.40),
    CColor(1.00, 0.995, 0.97), CColor(0.06, 0.06, 0.06),
    CColor(0.72, 0.72, 0.67), CColor(0.08, 0.44, 0.86),
    CColor(0.88, 0.36, 0.12), CColor(0.70, 0.70, 0.64)
};

const CColor classicDark[ThemeColorCount] = {
    CColor(0.70, 0.78, 0.68), CColor(0.32, 0.42, 0.33),
    CColor(0.86, 0.94, 0.82), CColor(0.35, 0.49, 0.35),
    CColor(0.53, 0.68, 1.00), CColor(0.96, 0.42, 0.52),
    CColor(0.96, 0.73, 0.20), CColor(0.055, 0.065, 0.060),
    CColor(0.36, 0.33, 0.29), CColor(0.24, 0.22, 0.20),
    CColor(1.00, 0.18, 0.16), CColor(0.90, 0.92, 0.88),
    CColor(0.94, 0.96, 0.92), CColor(1.00, 0.38, 0.18),
    CColor(0.82, 0.86, 0.80), CColor(0.10, 0.24, 0.34),
    CColor(0.28, 0.58, 0.70), CColor(0.16, 0.38, 0.50),
    CColor(0.025, 0.030, 0.030), CColor(0.16, 0.19, 0.17),
    CColor(0.00, 0.00, 0.00), CColor(0.58, 0.64, 0.58),
    CColor(0.88, 0.90, 0.84), CColor(0.04, 0.045, 0.045),
    CColor(0.33, 0.38, 0.34), CColor(0.34, 0.64, 1.00),
    CColor(1.00, 0.56, 0.22), CColor(0.24, 0.31, 0.27)
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
int Cfg::m_viewMode = PB_VIEW_MODE_score;

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
