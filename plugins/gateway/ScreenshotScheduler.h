/*
 * ScreenshotScheduler.h - periodic screenshots of the roaming laptops
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

#include <QDateTime>
#include <QHash>
#include <QTimer>

#include "ComputerControlInterface.h"

class GatewayService;

// Takes a screenshot of every roaming laptop every N minutes (as an audit
// trail) and stores it below <gateway directory>/screenshots/<laptop>/<date>/.
// The gateway connects to the laptop like a Master would - through the port
// forwarding - so it needs a private authentication key on this computer.
class ScreenshotScheduler : public QObject
{
	Q_OBJECT
public:
	explicit ScreenshotScheduler( GatewayService* service );
	~ScreenshotScheduler() override;

	static QString directory();

	// name of the key used, empty when this computer has no private key
	static QString availableKeyName( const QString& preferred );

private:
	struct Capture
	{
		ComputerControlInterface::Pointer control;
		QString laptop;
		QTimer* timeout{nullptr};
	};

	void tick();
	void capture( const QByteArray& agentKey, const QString& laptop, quint16 port );
	void finishCapture( const QByteArray& agentKey, const QImage& image );
	void prune();
	bool loadCredentials();

	static constexpr int TickInterval = 60 * 1000;
	static constexpr int CaptureTimeout = 60 * 1000;
	static constexpr int MaxImageWidth = 1600;

	GatewayService* m_service;
	QTimer m_timer;
	QHash<QByteArray, QDateTime> m_lastCapture;
	QHash<QByteArray, Capture> m_captures;
	QString m_loadedKey;
	QDate m_lastPrune;

};
