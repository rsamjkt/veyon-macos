/*
 * Updater.cpp - checks for, downloads and installs new AruniControl releases
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

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QProcess>

#include "PlatformFilesystemFunctions.h"
#include "PlatformServiceFunctions.h"
#include "UpdateState.h"
#include "Updater.h"
#include "VeyonCore.h"


Updater::Updater( QObject* parent ) :
	QObject( parent ),
	m_instanceLock( QDir( VeyonCore::platform().filesystemFunctions().globalTempPath() ).filePath( QStringLiteral("aruni-update.lock") ) )
{
	m_instanceLock.setStaleLockTime( 0 );
	m_network.setTransferTimeout( 60000 );

	// the first check waits a little so a freshly started computer (and an
	// update that just restarted the server) settles down first
	m_nextCheck = QDateTime::currentDateTimeUtc().addMSecs( FirstCheckDelay );

	connect( &m_pollTimer, &QTimer::timeout, this, &Updater::checkLoop );
	m_pollTimer.start( PollInterval );

	cleanUp();
}



Updater::~Updater()
{
	if( m_haveLock )
	{
		m_instanceLock.unlock();
	}
}



QString Updater::userAgent()
{
	return QStringLiteral("AruniControl/%1 (%2)").arg( VeyonCore::versionString(), platformKey() );
}



QString Updater::platformKey()
{
#if defined(Q_OS_WIN)
	return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
	return QStringLiteral("macos");
#else
	return {};
#endif
}



bool Updater::isNewer( const QString& version )
{
	const auto available = QVersionNumber::fromString( version );
	const auto current = QVersionNumber::fromString( VeyonCore::versionString() );
	return available.isNull() == false && QVersionNumber::compare( available, current ) > 0;
}



void Updater::checkLoop()
{
	if( platformKey().isEmpty() || m_busy )
	{
		return;
	}

	const bool requested = UpdateState::takeCheckRequest();
	const auto state = UpdateState::load();
	if( state.enabled == false && requested == false )
	{
		return;
	}

	if( requested == false && QDateTime::currentDateTimeUtc() < m_nextCheck )
	{
		return;
	}

	// only one server instance per computer updates
	if( m_haveLock == false )
	{
		m_haveLock = m_instanceLock.tryLock( 0 );
		if( m_haveLock == false )
		{
			return;
		}
	}

	m_nextCheck = QDateTime::currentDateTimeUtc().addMSecs( CheckInterval );
	check();
}



void Updater::check()
{
	m_busy = true;
	setStatus( QStringLiteral("checking") );

	QNetworkRequest request( QUrl( UpdateState::load().manifestUrl ) );
	request.setHeader( QNetworkRequest::UserAgentHeader, userAgent() );
	auto reply = m_network.get( request );
	connect( reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		m_lastCheck = QDateTime::currentDateTimeUtc();
		if( reply->error() != QNetworkReply::NoError )
		{
			m_busy = false;
			setStatus( QStringLiteral("error"), reply->errorString() );
			return;
		}
		onManifest( QJsonDocument::fromJson( reply->readAll() ).object() );
	} );
}



void Updater::onManifest( const QJsonObject& manifest )
{
	const auto version = manifest[QStringLiteral("version")].toString();
	const auto file = manifest[QStringLiteral("files")].toObject()[platformKey()].toObject();
	m_availableVersion = version;

	if( isNewer( version ) == false )
	{
		m_busy = false;
		setStatus( QStringLiteral("current") );
		return;
	}

	const auto url = QUrl( file[QStringLiteral("url")].toString() );
	if( url.scheme() != QStringLiteral("https") || file[QStringLiteral("sha256")].toString().size() != 64 )
	{
		m_busy = false;
		setStatus( QStringLiteral("error"), tr( "The release %1 has no package for this platform" ).arg( version ) );
		return;
	}

	vInfo() << "AruniControl update" << version << "available - downloading" << url;
	download( file, version );
}



void Updater::download( const QJsonObject& file, const QString& version )
{
	const auto folder = QDir( UpdateState::directory() ).filePath( QStringLiteral("download") );
	QDir( folder ).removeRecursively();
	QDir().mkpath( folder );

	const auto name = QFileInfo( file[QStringLiteral("name")].toString() ).fileName();
	m_downloadFile = std::make_unique<QFile>( QDir( folder ).filePath( name.isEmpty() ? QStringLiteral("package") : name ) );
	if( m_downloadFile->open( QFile::WriteOnly | QFile::Truncate ) == false )
	{
		m_busy = false;
		setStatus( QStringLiteral("error"), tr( "Cannot write %1" ).arg( m_downloadFile->fileName() ) );
		return;
	}
	m_downloadHash = std::make_unique<QCryptographicHash>( QCryptographicHash::Sha256 );

	setStatus( QStringLiteral("downloading") );

	QNetworkRequest request( QUrl( file[QStringLiteral("url")].toString() ) );
	request.setAttribute( QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy );
	request.setHeader( QNetworkRequest::UserAgentHeader, userAgent() );
	// large package: the timeout applies to stalls, not to the whole transfer
	request.setTransferTimeout( 120000 );
	auto reply = m_network.get( request );

	connect( reply, &QNetworkReply::readyRead, this, [this, reply]() {
		const auto data = reply->readAll();
		m_downloadHash->addData( data );
		m_downloadFile->write( data );
	} );

	const auto expected = file[QStringLiteral("sha256")].toString().toLower();
	connect( reply, &QNetworkReply::finished, this, [this, reply, expected, version]() {
		reply->deleteLater();
		const auto data = reply->readAll();
		m_downloadHash->addData( data );
		m_downloadFile->write( data );
		m_downloadFile->close();

		const auto path = m_downloadFile->fileName();
		const auto hash = QString::fromLatin1( m_downloadHash->result().toHex() );
		m_downloadFile.reset();
		m_downloadHash.reset();

		if( reply->error() != QNetworkReply::NoError )
		{
			m_busy = false;
			setStatus( QStringLiteral("error"), reply->errorString() );
			return;
		}

		// never run anything that is not exactly the published package
		if( hash != expected )
		{
			QFile::remove( path );
			m_busy = false;
			setStatus( QStringLiteral("error"), tr( "The download of version %1 is damaged (checksum mismatch)" ).arg( version ) );
			return;
		}

		install( path, version );
	} );
}



void Updater::install( const QString& packagePath, const QString& version )
{
	setStatus( QStringLiteral("installing") );
	vInfo() << "installing AruniControl" << version << "from" << packagePath;

#if defined(Q_OS_WIN)
	const bool started = installWindows( packagePath );
#elif defined(Q_OS_MACOS)
	const bool started = installMac( packagePath );
#else
	const bool started = false;
#endif

	if( started == false )
	{
		m_busy = false;
		if( UpdateState::readStatus()[QStringLiteral("state")].toString() != QStringLiteral("error") )
		{
			setStatus( QStringLiteral("error"), tr( "The update could not be installed" ) );
		}
	}
	// on success this process is ended by the installation
}



bool Updater::installWindows( const QString& packagePath )
{
	// the installer stops the service (which ends this server as well),
	// replaces the files and starts everything again
	return QProcess::startDetached( packagePath, { QStringLiteral("/S"), QStringLiteral("/UPDATE") } );
}



bool Updater::installMac( const QString& packagePath )
{
	// .../AruniControl.app/Contents/MacOS/veyon-server
	QDir bundleDir( QCoreApplication::applicationDirPath() );
	bundleDir.cdUp();
	bundleDir.cdUp();
	const auto bundlePath = bundleDir.absolutePath();
	if( bundlePath.endsWith( QStringLiteral(".app") ) == false )
	{
		setStatus( QStringLiteral("error"), tr( "Not running from an application bundle - update skipped" ) );
		return false;
	}

	if( QFileInfo( QFileInfo( bundlePath ).absolutePath() ).isWritable() == false )
	{
		setStatus( QStringLiteral("error"), tr( "No permission to replace %1" ).arg( bundlePath ) );
		return false;
	}

	const auto extractDir = QDir( UpdateState::directory() ).filePath( QStringLiteral("extract") );
	QDir( extractDir ).removeRecursively();
	QDir().mkpath( extractDir );

	if( QProcess::execute( QStringLiteral("/usr/bin/ditto"), { QStringLiteral("-x"), QStringLiteral("-k"), packagePath, extractDir } ) != 0 )
	{
		setStatus( QStringLiteral("error"), tr( "The update package could not be unpacked" ) );
		return false;
	}

	const auto newBundle = QDir( extractDir ).filePath( QFileInfo( bundlePath ).fileName() );
	if( QFileInfo::exists( newBundle + QStringLiteral("/Contents/MacOS/veyon-server") ) == false )
	{
		setStatus( QStringLiteral("error"), tr( "The update package does not contain %1" ).arg( QFileInfo( bundlePath ).fileName() ) );
		return false;
	}

	// swap the bundles; the old one is removed on the next start (cleanUp())
	const auto oldBundle = bundlePath + QStringLiteral(".old");
	QDir( oldBundle ).removeRecursively();
	if( QDir().rename( bundlePath, oldBundle ) == false )
	{
		setStatus( QStringLiteral("error"), tr( "No permission to replace %1" ).arg( bundlePath ) );
		return false;
	}
	if( QDir().rename( newBundle, bundlePath ) == false )
	{
		QDir().rename( oldBundle, bundlePath );
		setStatus( QStringLiteral("error"), tr( "The new version could not be put in place" ) );
		return false;
	}

	// launchd restarts the server from the new bundle - this process ends
	const auto label = VeyonCore::platform().serviceFunctions().veyonServiceName();
	return QProcess::startDetached( QStringLiteral("/bin/sh"), {
		QStringLiteral("-c"),
		QStringLiteral("sleep 2; launchctl kickstart -k \"gui/$(id -u)/%1\"").arg( label ) } );
}



void Updater::setStatus( const QString& state, const QString& error )
{
	if( error.isEmpty() == false )
	{
		vWarning() << "AruniControl update:" << error;
	}

	UpdateState::writeStatus( {
		{ QStringLiteral("state"), state },
		{ QStringLiteral("error"), error },
		{ QStringLiteral("current"), VeyonCore::versionString() },
		{ QStringLiteral("available"), m_availableVersion },
		{ QStringLiteral("checked"), m_lastCheck.toString( Qt::ISODate ) },
		{ QStringLiteral("next"), m_nextCheck.toString( Qt::ISODate ) },
		{ QStringLiteral("updated"), QDateTime::currentDateTimeUtc().toString( Qt::ISODate ) },
	} );
}



void Updater::cleanUp()
{
#if defined(Q_OS_MACOS)
	QDir bundleDir( QCoreApplication::applicationDirPath() );
	bundleDir.cdUp();
	bundleDir.cdUp();
	if( bundleDir.absolutePath().endsWith( QStringLiteral(".app") ) )
	{
		QDir( bundleDir.absolutePath() + QStringLiteral(".old") ).removeRecursively();
	}
	QDir( QDir( UpdateState::directory() ).filePath( QStringLiteral("extract") ) ).removeRecursively();
#endif
}
