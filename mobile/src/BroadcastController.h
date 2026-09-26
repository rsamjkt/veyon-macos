/*
 * BroadcastController.h - voice broadcast (announcement) to many computers
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

#include <QPointer>
#include <QTimer>

#include "FeatureSession.h"

class AudioEngine;

// AruniVoice broadcast: the teacher speaks to all target computers at once
// (hold-to-talk or an open microphone). One-way - nothing comes back and the
// computers never open their microphones; they only show a small notice.
class BroadcastController : public FeatureSession
{
	Q_OBJECT
	Q_PROPERTY(bool active READ isActive NOTIFY targetsChanged)
	Q_PROPERTY(QString targetLabel READ targetLabel NOTIFY targetsChanged)
	Q_PROPERTY(int targetCount READ targetCount NOTIFY targetsChanged)
	Q_PROPERTY(int onlineCount READ onlineCount NOTIFY targetsChanged)
	Q_PROPERTY(bool talking READ isTalking NOTIFY stateChanged)
	Q_PROPERTY(bool keepOpen READ isKeepOpen NOTIFY stateChanged)
	Q_PROPERTY(bool microphoneMissing READ isMicrophoneMissing NOTIFY stateChanged)
	Q_PROPERTY(QString permission READ permission NOTIFY permissionChanged)
	Q_PROPERTY(qreal micLevel READ micLevel NOTIFY levelChanged)
public:
	explicit BroadcastController( ComputerGridModel* computers, QObject* parent = nullptr );
	~BroadcastController() override;

	bool isActive() const
	{
		return m_active;
	}

	const QString& targetLabel() const
	{
		return m_label;
	}

	int targetCount() const;
	int onlineCount() const;

	bool isTalking() const
	{
		return m_talking;
	}

	bool isKeepOpen() const
	{
		return m_keepOpen;
	}

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

	// an empty uid list means "all visible computers"
	Q_INVOKABLE void open( const QStringList& uids, const QString& label );
	Q_INVOKABLE void end();
	Q_INVOKABLE void startTalking();
	Q_INVOKABLE void stopTalking();
	Q_INVOKABLE void setKeepOpen( bool enabled );
	Q_INVOKABLE void requestPermission();

Q_SIGNALS:
	void targetsChanged();
	void stateChanged();
	void permissionChanged();
	void levelChanged();
	void problem( const QString& message );

protected:
	void handleMessage( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message ) override;

private:
	ComputerControlInterfaceList targets() const;
	bool startCapture();
	void stopCapture();
	void sendChunk( const QByteArray& pcm );
	void flush();

	QPointer<AudioEngine> m_engine;
	QStringList m_uids;
	QString m_label;
	ComputerControlInterfaceList m_audience;	// resolved when the microphone opens
	QByteArray m_buffer;
	bool m_active{false};
	bool m_talking{false};
	bool m_keepOpen{false};
	bool m_microphoneMissing{false};
	qreal m_micLevel{0};
	QTimer m_levelTimer;
	QTimer m_onlineTimer;

};
