/*!
    @file           GuiLoopingPopup.cpp

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

#include <QtWidgets>

#include "ApplicationController.h"
#include "GuiLoopingPopup.h"

GuiLoopingPopup::GuiLoopingPopup(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
    m_controller = nullptr;
    setWindowTitle(tr("Continuous Looping"));
    setWindowFlags(Qt::Popup);
}

void GuiLoopingPopup::init(ApplicationController* controller)
{
    m_controller = controller;
    loopBarsSpin->setValue(m_controller->loopingBars());
    updateInfo();
}

void GuiLoopingPopup::updateInfo()
{
    if (!m_controller)
        return;

    if (m_controller->loopingBars() > 0.0)
        loopingText->setText(tr("Repeat End Bar:") + " " + QString().setNum(m_controller->playUptoBarPosition()));
    else
        loopingText->setText(tr("Repeat Bar is disabled"));
}

void GuiLoopingPopup::on_loopBarsSpin_valueChanged(double bars)
{
    if (!m_controller) return;

    m_controller->setLoopingBars(bars);
    updateInfo();
}

void GuiLoopingPopup::closeEvent(QCloseEvent *event)
{
    Q_UNUSED(event)
    QPushButton* parent = static_cast<QPushButton*> (parentWidget());
    if (parent)
        parent->setChecked(false);
}
