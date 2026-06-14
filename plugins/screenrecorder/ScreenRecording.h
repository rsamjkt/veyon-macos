/*
 * ScreenRecording.h - records one computer's framebuffer to a video file
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

#include <QObject>

#include "ComputerControlInterface.h"

class MacVideoWriter;
class QTimer;

class ScreenRecording : public QObject
{
	Q_OBJECT
public:
	ScreenRecording( const ComputerControlInterface::Pointer& controlInterface, QObject* parent = nullptr );
	~ScreenRecording() override;

	QString filePath() const
	{
		return m_filePath;
	}

	static QString outputDirectory();

private:
	void captureFrame();

	ComputerControlInterface::Pointer m_controlInterface;
	MacVideoWriter* m_writer;
	QTimer* m_timer;
	QString m_filePath;
	bool m_started{false};

	static constexpr int RecordingFps = 8;

};
