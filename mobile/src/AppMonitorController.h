/*
 * AppMonitorController.h - running applications of one computer
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

#include <QStringList>
#include <QTimer>

#include "FeatureSession.h"

// Running applications of one computer (ApplicationMonitoring plugin): polls
// the list while the page is open, like the desktop dialog, and can close an
// application.
class AppMonitorController : public FeatureSession
{
	Q_OBJECT
	Q_PROPERTY(QStringList applications READ applications NOTIFY applicationsChanged)
	Q_PROPERTY(QString frontmost READ frontmost NOTIFY applicationsChanged)
	Q_PROPERTY(QString state READ state NOTIFY stateChanged)
	Q_PROPERTY(QString updatedAt READ updatedAt NOTIFY applicationsChanged)
public:
	explicit AppMonitorController( ComputerGridModel* computers, QObject* parent = nullptr );

	const QStringList& applications() const
	{
		return m_applications;
	}

	const QString& frontmost() const
	{
		return m_frontmost;
	}

	// "loading", "ready", "offline" or "noresponse"
	const QString& state() const
	{
		return m_state;
	}

	const QString& updatedAt() const
	{
		return m_updatedAt;
	}

	Q_INVOKABLE void open( const QString& uid );
	Q_INVOKABLE void close();
	Q_INVOKABLE void refresh();
	Q_INVOKABLE bool terminate( const QString& application );

Q_SIGNALS:
	void applicationsChanged();
	void stateChanged();

protected:
	void handleMessage( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message ) override;

private:
	void poll();
	void setState( const QString& state );

	QStringList m_applications;
	QString m_frontmost;
	QString m_state;
	QString m_updatedAt;
	QTimer m_pollTimer;
	QTimer m_timeoutTimer;

};
