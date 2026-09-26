/*
 * VoiceController.h - AruniVoice push-to-talk / intercom with one computer
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

#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>

#include "FeatureSession.h"

class AudioEngine;

// AruniVoice with one computer: push-to-talk / open intercom. Uses the
// plugin's AudioEngine (16 kHz mono PCM) and its feature messages.
class VoiceController : public FeatureSession
{
	Q_OBJECT
	Q_PROPERTY(bool active READ isActive NOTIFY computerChanged)
	Q_PROPERTY(bool talking READ isTalking NOTIFY stateChanged)
	Q_PROPERTY(bool intercom READ isIntercom NOTIFY stateChanged)
	Q_PROPERTY(bool speakerMuted READ isSpeakerMuted WRITE setSpeakerMuted NOTIFY stateChanged)
	Q_PROPERTY(bool microphoneMissing READ isMicrophoneMissing NOTIFY stateChanged)
	Q_PROPERTY(QString permission READ permission NOTIFY permissionChanged)
	Q_PROPERTY(qreal micLevel READ micLevel NOTIFY levelsChanged)
	Q_PROPERTY(qreal remoteLevel READ remoteLevel NOTIFY levelsChanged)
	Q_PROPERTY(bool remoteSpeaking READ isRemoteSpeaking NOTIFY levelsChanged)
public:
	explicit VoiceController( ComputerGridModel* computers, QObject* parent = nullptr );
	~VoiceController() override;

	bool isActive() const
	{
		return computerUid().isEmpty() == false;
	}

	bool isTalking() const
	{
		return m_talking;
	}

	bool isIntercom() const
	{
		return m_intercom;
	}

	bool isSpeakerMuted() const
	{
		return m_speakerMuted;
	}
	void setSpeakerMuted( bool muted );

	bool isMicrophoneMissing() const
	{
		return m_microphoneMissing;
	}

	// "granted", "denied" or "undetermined"
	QString permission() const;

	qreal micLevel() const
	{
		return m_micLevel;
	}

	qreal remoteLevel() const
	{
		return m_remoteLevel;
	}

	bool isRemoteSpeaking() const;

	Q_INVOKABLE void open( const QString& uid );
	Q_INVOKABLE void end();
	Q_INVOKABLE void startTalking();
	Q_INVOKABLE void stopTalking();
	Q_INVOKABLE void setIntercom( bool enabled );
	Q_INVOKABLE void requestPermission();
	Q_INVOKABLE void openPermissionSettings();

Q_SIGNALS:
	void stateChanged();
	void permissionChanged();
	void levelsChanged();
	void problem( const QString& message );

protected:
	void handleMessage( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message ) override;

private:
	bool startCapture();
	void stopCapture();
	void setTalking( bool talking );
	void decayLevels();

	static qreal peakLevel( const QByteArray& pcm );

	QPointer<AudioEngine> m_engine;
	bool m_talking{false};
	bool m_intercom{false};
	bool m_speakerMuted{false};
	bool m_microphoneMissing{false};
	qreal m_micLevel{0};
	qreal m_remoteLevel{0};
	QElapsedTimer m_lastRemoteAudio;
	QTimer m_levelTimer;

};
