/*
 * AudioEngine.cpp - microphone capture + speaker playback for AruniVoice
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

#include <QAudioSource>
#include <QAudioSink>
#include <QAudioFormat>
#include <QMediaDevices>
#include <QIODevice>

#include "AudioEngine.h"
#include "VeyonCore.h"


static QAudioFormat voiceFormat()
{
	QAudioFormat format;
	format.setSampleRate( 16000 );
	format.setChannelCount( 1 );
	format.setSampleFormat( QAudioFormat::Int16 );
	return format;
}



AudioEngine::AudioEngine( QObject* parent ) :
	QObject( parent )
{
}



AudioEngine::~AudioEngine()
{
	stopCapture();
	delete m_sink;
	delete m_source;
}



void AudioEngine::startCapture()
{
	if( m_sourceIo != nullptr )
	{
		return; // already capturing
	}

	const auto device = QMediaDevices::defaultAudioInput();
	if( device.isNull() )
	{
		vWarning() << "AruniVoice: no audio input device";
		return;
	}

	auto format = voiceFormat();
	if( device.isFormatSupported( format ) == false )
	{
		format = device.preferredFormat();
	}

	if( m_source == nullptr )
	{
		m_source = new QAudioSource( device, format, this );
	}

	m_sourceIo = m_source->start();
	if( m_sourceIo == nullptr )
	{
		vWarning() << "AruniVoice: failed to start microphone";
		return;
	}

	connect( m_sourceIo, &QIODevice::readyRead, this, [this]() {
		if( m_sourceIo == nullptr )
		{
			return;
		}
		const QByteArray data = m_sourceIo->readAll();
		if( data.isEmpty() == false )
		{
			Q_EMIT chunkCaptured( data );
		}
	} );
}



void AudioEngine::stopCapture()
{
	if( m_source != nullptr )
	{
		m_source->stop();
	}
	m_sourceIo = nullptr;
}



void AudioEngine::ensurePlayback()
{
	if( m_sinkIo != nullptr )
	{
		return;
	}

	const auto device = QMediaDevices::defaultAudioOutput();
	if( device.isNull() )
	{
		vWarning() << "AruniVoice: no audio output device";
		return;
	}

	auto format = voiceFormat();
	if( device.isFormatSupported( format ) == false )
	{
		format = device.preferredFormat();
	}

	if( m_sink == nullptr )
	{
		m_sink = new QAudioSink( device, format, this );
	}

	m_sinkIo = m_sink->start();   // push mode: write PCM to the returned device
}



void AudioEngine::playChunk( const QByteArray& pcm )
{
	ensurePlayback();
	if( m_sinkIo != nullptr && pcm.isEmpty() == false )
	{
		m_sinkIo->write( pcm );
	}
}
