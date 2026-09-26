/*
 * BroadcastNotice.h - unobtrusive "teacher is speaking" notice
 *
 * Copyright (c) 2026 Arunika / AruniControl
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

#include <QLabel>
#include <QTimer>

// Small frameless, always-on-top label shown on the computers while a voice
// broadcast is playing. It never takes the focus or mouse input and hides
// itself a few seconds after the last audio chunk.
class BroadcastNotice : public QLabel
{
	Q_OBJECT
public:
	explicit BroadcastNotice( QWidget* parent = nullptr );

	// (re)shows the notice and restarts the hide timer
	void ping( const QString& speaker );

protected:
	void paintEvent( QPaintEvent* event ) override;

private:
	void placeOnScreen();

	static constexpr int HideDelay = 3000;

	QTimer m_hideTimer;

};
