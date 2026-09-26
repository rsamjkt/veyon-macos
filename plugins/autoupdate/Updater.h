/*
 * Updater.h - checks for, downloads and installs new AruniControl releases
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
#include <QDateTime>
#include <QFile>
#include <QJsonObject>
#include <QLockFile>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <QVersionNumber>

#include <memory>

// Runs in veyon-server (one instance per computer, lock file). Every few hours
// it reads the release manifest of the download website (/unduh/versi.json);
// when it lists a newer version for this platform it downloads the package,
// verifies its SHA-256 and installs it:
// - Windows: runs the installer silently (it stops the service, replaces the
//   files and starts the service again)
// - macOS: swaps AruniControl.app for the new bundle and restarts the server
//   through launchd
class Updater : public QObject
{
	Q_OBJECT
public:
	explicit Updater( QObject* parent = nullptr );
	~Updater() override;

	static QString platformKey();
	static QString userAgent();
	static bool isNewer( const QString& version );

private:
	void checkLoop();
	void check();
	void onManifest( const QJsonObject& manifest );
	void download( const QJsonObject& file, const QString& version );
	void install( const QString& packagePath, const QString& version );
	bool installWindows( const QString& packagePath );
	bool installMac( const QString& packagePath );
	void setStatus( const QString& state, const QString& error = {} );
	void cleanUp();

	static constexpr int CheckInterval = 6 * 60 * 60 * 1000;
	static constexpr int FirstCheckDelay = 3 * 60 * 1000;
	static constexpr int PollInterval = 5000;

	QLockFile m_instanceLock;
	bool m_haveLock{false};
	QNetworkAccessManager m_network;
	QTimer m_pollTimer;
	QDateTime m_nextCheck;
	bool m_busy{false};

	QString m_availableVersion;
	QDateTime m_lastCheck;

	std::unique_ptr<QFile> m_downloadFile;
	std::unique_ptr<QCryptographicHash> m_downloadHash;

};
