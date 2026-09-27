/*
 * AppUpdater.h - updates of the Android app from the download website
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

#include <QCryptographicHash>
#include <QFile>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTimer>

#include <memory>

// Reads /unduh/versi.json of the download website (like the updater of the
// desktop versions). When a newer APK is published the home page offers it;
// the APK is downloaded, verified (SHA-256) and handed to the Android package
// installer, which asks the user to confirm.
class AppUpdater : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool available READ isAvailable NOTIFY changed)
	Q_PROPERTY(QString version READ version NOTIFY changed)
	Q_PROPERTY(QString state READ state NOTIFY changed)		// idle, downloading, installing, error
	Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
	Q_PROPERTY(QString error READ error NOTIFY changed)
	Q_PROPERTY(bool dismissed READ isDismissed NOTIFY changed)
public:
	static constexpr auto ManifestUrl = "https://arunicontrol.arunihealth.id/unduh/versi.json";

	explicit AppUpdater( QObject* parent = nullptr );

	bool isAvailable() const
	{
		return m_version.isEmpty() == false;
	}
	const QString& version() const
	{
		return m_version;
	}
	const QString& state() const
	{
		return m_state;
	}
	double progress() const
	{
		return m_progress;
	}
	const QString& error() const
	{
		return m_error;
	}
	bool isDismissed() const;

	Q_INVOKABLE void check();
	Q_INVOKABLE void update();
	Q_INVOKABLE void dismiss();

Q_SIGNALS:
	void changed();
	void progressChanged();

private:
	void setState( const QString& state, const QString& error = {} );
	void install( const QString& path );
	static QString userAgent();

	static constexpr int CheckInterval = 12 * 60 * 60 * 1000;

	QNetworkAccessManager m_network;
	QTimer m_timer;
	QString m_version;
	QString m_url;
	QString m_sha256;
	QString m_readyPath;
	QString m_state{QStringLiteral("idle")};
	QString m_error;
	double m_progress{0};
	std::unique_ptr<QFile> m_file;
	std::unique_ptr<QCryptographicHash> m_hash;

};
