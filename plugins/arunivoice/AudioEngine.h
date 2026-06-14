/*
 * AudioEngine.h - microphone capture + speaker playback for AruniVoice
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

QT_FORWARD_DECLARE_CLASS(QAudioSource)
QT_FORWARD_DECLARE_CLASS(QAudioSink)
QT_FORWARD_DECLARE_CLASS(QIODevice)

// Captures the default microphone and plays received audio on the default
// output, using a fixed voice-grade PCM format (16 kHz, mono, 16-bit signed).
// ~32 KB/s on the wire — trivial for a classroom LAN.
class AudioEngine : public QObject
{
	Q_OBJECT
public:
	explicit AudioEngine( QObject* parent = nullptr );
	~AudioEngine() override;

	void startCapture();
	void stopCapture();
	bool isCapturing() const { return m_sourceIo != nullptr; }

	// Queue a received PCM chunk for playback (starts the output on first use).
	void playChunk( const QByteArray& pcm );

Q_SIGNALS:
	void chunkCaptured( const QByteArray& pcm );

private:
	void ensurePlayback();

	QAudioSource* m_source = nullptr;
	QIODevice* m_sourceIo = nullptr;   // non-owning (owned by m_source)
	QAudioSink* m_sink = nullptr;
	QIODevice* m_sinkIo = nullptr;     // non-owning (owned by m_sink)

};
