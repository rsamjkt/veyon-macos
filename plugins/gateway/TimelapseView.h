/*
 * TimelapseView.h - plays back the scheduled screenshots of a computer
 *
 * Copyright (c) 2026 AruniControl Community
 *
 * This file is part of Veyon - https://veyon.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#pragma once

#include <QTimer>
#include <QWidget>

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSlider;

// A day of screenshots of one computer as a timelapse: computer list, date,
// play/pause with speed, a slider to scrub and the time of the frame.
class TimelapseView : public QWidget
{
	Q_OBJECT
public:
	explicit TimelapseView( QWidget* parent = nullptr );

	void refresh();

protected:
	void resizeEvent( QResizeEvent* event ) override;

private:
	void loadComputer();
	void loadDay();
	void showFrame( int index );
	void togglePlayback();

	QListWidget* m_computers;
	QComboBox* m_days;
	QLabel* m_image;
	QLabel* m_time;
	QSlider* m_slider;
	QPushButton* m_playButton;
	QComboBox* m_speed;
	QTimer m_playTimer;
	QStringList m_frames;
	QString m_folder;

};
