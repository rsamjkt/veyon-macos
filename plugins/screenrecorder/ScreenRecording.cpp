/*
 * ScreenRecording.cpp - records one computer's framebuffer to a video file
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

#include <QDateTime>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

#include "ScreenRecording.h"
#include "MacVideoWriter.h"


QString ScreenRecording::outputDirectory()
{
	auto base = QStandardPaths::writableLocation( QStandardPaths::MoviesLocation );
	if( base.isEmpty() )
	{
		base = QDir::homePath();
	}
	return base + QStringLiteral("/AruniControl");
}



ScreenRecording::ScreenRecording( const ComputerControlInterface::Pointer& controlInterface, QObject* parent ) :
	QObject( parent ),
	m_controlInterface( controlInterface ),
	m_writer( new MacVideoWriter ),
	m_timer( new QTimer( this ) )
{
	const auto dir = outputDirectory();
	QDir().mkpath( dir );

	auto computerName = m_controlInterface->computer().displayName();
	computerName.replace( QRegularExpression( QStringLiteral("[^A-Za-z0-9_.-]") ), QStringLiteral("_") );
	if( computerName.isEmpty() )
	{
		computerName = QStringLiteral("computer");
	}

	m_filePath = QStringLiteral("%1/%2_%3.mov")
					 .arg( dir, computerName,
						   QDateTime::currentDateTime().toString( QStringLiteral("yyyyMMdd-HHmmss") ) );

	connect( m_timer, &QTimer::timeout, this, &ScreenRecording::captureFrame );
	m_timer->start( 1000 / RecordingFps );
}



ScreenRecording::~ScreenRecording()
{
	m_timer->stop();
	if( m_writer->isOpen() )
	{
		m_writer->close();
		vInfo() << "ScreenRecording: saved" << m_filePath;
	}
	delete m_writer;
}



void ScreenRecording::captureFrame()
{
	const QImage framebuffer = m_controlInterface->framebuffer();
	if( framebuffer.isNull() || framebuffer.width() < 2 || framebuffer.height() < 2 )
	{
		return; // wait until a valid framebuffer is available
	}

	if( m_started == false )
	{
		if( m_writer->open( m_filePath, framebuffer.size(), RecordingFps ) == false )
		{
			vCritical() << "ScreenRecording: could not start recording to" << m_filePath;
			m_timer->stop();
			return;
		}
		m_started = true;
		vInfo() << "ScreenRecording: recording" << m_controlInterface->computer().displayName()
				<< "to" << m_filePath;
	}

	m_writer->writeFrame( framebuffer );
}
