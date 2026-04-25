/*********************************************************************************/
/*!
@file           GuiSidePanel.h

@brief          xxxx.

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

#ifndef __GUISIDEPANEL_H__
#define __GUISIDEPANEL_H__

#include <QtWidgets>

#include "Chord.h"

#include "ui_GuiSidePanel.h"

class ApplicationController;
class CSettings;
class CTrackList;
class GuiTopBar;

class GuiSidePanel : public QWidget, private Ui::GuiSidePanel
{
    Q_OBJECT

public:
    GuiSidePanel(QWidget *parent, CSettings* settings);

    void init(ApplicationController* controller, GuiTopBar* topBar);

    void refresh();

    void loadBookList();
    void setBookName(const QString &bookName);
    void setSongName(const QString &songName);
    int getSongIndex() {return songCombo->currentIndex();}
    void setSongIndex(int index){songCombo->setCurrentIndex(index);}
    void setCurrentHand(const QString &hand);

    bool isRepeatSong(){return repeatSong->isChecked();}

    void updateTranslate();

    void setActiveHand(whichPart_t hand)
    {
        if (hand == PB_PART_right) rightHandRadio->setChecked(true);
        if (hand == PB_PART_both)  bothHandsRadio->setChecked(true);
        if (hand == PB_PART_left)  leftHandRadio->setChecked(true);
    }

    void nextSong(int amount)
    {
        int n = songCombo->currentIndex() + amount;
        if (n < 0 || n >= songCombo->count())
            return;

        songCombo->setCurrentIndex(n);
        on_songCombo_activated(n);
    }

    void nextBook(int amount)
    {
        int n = bookCombo->currentIndex() + amount;
        if (n < 0 || n >= bookCombo->count())
            return;

        bookCombo->setCurrentIndex(n);
        on_bookCombo_activated(n);
    }

private slots:
    void on_songCombo_activated (int index);
    void on_bookCombo_activated (int index);
    void on_rightHandRadio_toggled (bool checked);
    void on_bothHandsRadio_toggled (bool checked);
    void on_leftHandRadio_toggled (bool checked);
    void on_repeatSong_released();
    void on_trackListWidget_currentRowChanged(int currentRow);
    void on_boostSlider_valueChanged(int value);
    void on_pianoSlider_valueChanged(int value);
    void on_listenRadio_toggled (bool checked);
    void on_rhythmTapRadio_toggled (bool checked);
    void on_followYouRadio_toggled (bool checked);
    void on_playAlongRadio_toggled (bool checked);
    void on_rhythmTappingCombo_activated (int index);
    void on_muteYourPartCheck_toggled (bool checked);
    void setTrackRightHandPart();
    void setTrackLeftHandPart();
    void clearTrackPart();

private:
    void autoSetMuteYourPart();

    QMap<QWidget*,QMap<QString,QString>> listWidgetsRetranslateUi;
    QMap<QAction*,QMap<QString,QString>> listActionsRetranslateUi;

    ApplicationController* m_controller;
    CTrackList* m_trackList;
    GuiTopBar* m_topBar;
    CSettings* m_settings;
    QWidget *m_parent;
};

#endif //__GUISIDEPANEL_H__
