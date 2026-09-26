/*
 * AppUpdater.cpp - updates of the Android app from the download website
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

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSettings>
#include <QStandardPaths>
#include <QVersionNumber>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

#include "AppUpdater.h"
#include "VeyonCore.h"


AppUpdater::AppUpdater( QObject* parent ) :
	QObject( parent )
{
	m_network.setTransferTimeout( 60000 );

	connect( &m_timer, &QTimer::timeout, this, &AppUpdater::check );
	m_timer.start( CheckInterval );

	// shortly after start, not during it
	QTimer::singleShot( 8000, this, &AppUpdater::check );
}



bool AppUpdater::isDismissed() const
{
	// "Nanti" hides the banner for this version until the next app start
	return m_version.isEmpty() == false &&
		   QSettings().value( QStringLiteral("Update/dismissed") ).toString() == m_version &&
		   m_state == QStringLiteral("idle");
}



QString AppUpdater::userAgent()
{
	return QStringLiteral("AruniControl/%1 (android)").arg( VeyonCore::versionString() );
}



void AppUpdater::check()
{
#ifdef Q_OS_ANDROID
	if( m_state != QStringLiteral("idle") && m_state != QStringLiteral("error") )
	{
		return;
	}

	QNetworkRequest request( QUrl( QString::fromLatin1( ManifestUrl ) ) );
	request.setHeader( QNetworkRequest::UserAgentHeader, userAgent() );
	auto reply = m_network.get( request );
	connect( reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		if( reply->error() != QNetworkReply::NoError )
		{
			return;	// offline - try again later, silently
		}

		const auto manifest = QJsonDocument::fromJson( reply->readAll() ).object();
		const auto version = manifest[QStringLiteral("version")].toString();
		const auto file = manifest[QStringLiteral("files")].toObject()[QStringLiteral("android")].toObject();
		const auto newer = QVersionNumber::compare( QVersionNumber::fromString( version ),
													QVersionNumber::fromString( VeyonCore::versionString() ) ) > 0;

		if( newer && file[QStringLiteral("url")].toString().startsWith( QStringLiteral("https://") ) &&
			file[QStringLiteral("sha256")].toString().size() == 64 )
		{
			if( version != m_version )
			{
				m_readyPath.clear();
			}
			m_version = version;
			m_url = file[QStringLiteral("url")].toString();
			m_sha256 = file[QStringLiteral("sha256")].toString().toLower();
		}
		else
		{
			m_version.clear();
		}
		Q_EMIT changed();
	} );
#endif
}



void AppUpdater::dismiss()
{
	QSettings().setValue( QStringLiteral("Update/dismissed"), m_version );
	Q_EMIT changed();
}



void AppUpdater::update()
{
	if( isAvailable() == false || m_state == QStringLiteral("downloading") )
	{
		return;
	}

	// already downloaded and verified (e.g. the first attempt had to ask for
	// the permission to install apps)
	if( m_readyPath.isEmpty() == false && QFile::exists( m_readyPath ) )
	{
		install( m_readyPath );
		return;
	}

	const auto folder = QDir( QStandardPaths::writableLocation( QStandardPaths::AppDataLocation ) ).filePath( QStringLiteral("update") );
	QDir( folder ).removeRecursively();
	QDir().mkpath( folder );

	m_file = std::make_unique<QFile>( QDir( folder ).filePath( QStringLiteral("AruniControl-%1.apk").arg( m_version ) ) );
	if( m_file->open( QFile::WriteOnly | QFile::Truncate ) == false )
	{
		setState( QStringLiteral("error"), tr("Tidak dapat menyimpan unduhan.") );
		return;
	}
	m_hash = std::make_unique<QCryptographicHash>( QCryptographicHash::Sha256 );
	m_progress = 0;
	Q_EMIT progressChanged();
	setState( QStringLiteral("downloading") );

	QNetworkRequest request( ( QUrl( m_url ) ) );
	request.setHeader( QNetworkRequest::UserAgentHeader, userAgent() );
	request.setTransferTimeout( 120000 );
	auto reply = m_network.get( request );

	connect( reply, &QNetworkReply::readyRead, this, [this, reply]() {
		const auto data = reply->readAll();
		m_hash->addData( data );
		m_file->write( data );
	} );
	connect( reply, &QNetworkReply::downloadProgress, this, [this]( qint64 received, qint64 total ) {
		if( total > 0 )
		{
			m_progress = double( received ) / double( total );
			Q_EMIT progressChanged();
		}
	} );
	connect( reply, &QNetworkReply::finished, this, [this, reply]() {
		reply->deleteLater();
		const auto data = reply->readAll();
		m_hash->addData( data );
		m_file->write( data );
		m_file->close();
		const auto path = m_file->fileName();
		const auto hash = QString::fromLatin1( m_hash->result().toHex() );
		m_file.reset();
		m_hash.reset();

		if( reply->error() != QNetworkReply::NoError )
		{
			setState( QStringLiteral("error"), tr("Unduhan gagal: %1").arg( reply->errorString() ) );
			return;
		}
		if( hash != m_sha256 )
		{
			QFile::remove( path );
			setState( QStringLiteral("error"), tr("File unduhan rusak (checksum tidak cocok). Coba lagi.") );
			return;
		}

		m_readyPath = path;
		install( path );
	} );
}



void AppUpdater::install( const QString& path )
{
#ifdef Q_OS_ANDROID
	const auto result = QJniObject::callStaticMethod<jint>( "id/arunika/arunicontrol/UpdateHelper", "installApk",
															"(Landroid/content/Context;Ljava/lang/String;)I",
															QJniObject( QNativeInterface::QAndroidApplication::context() ).object(),
															QJniObject::fromString( path ).object<jstring>() );
	switch( result )
	{
	case 0:
		// Android shows its installer; the app is replaced when confirmed
		setState( QStringLiteral("installing") );
		break;
	case 1:
		setState( QStringLiteral("error"),
				  tr("Izinkan AruniControl memasang aplikasi (halaman pengaturan sudah dibuka), lalu ketuk Perbarui lagi.") );
		break;
	default:
		setState( QStringLiteral("error"), tr("Pemasang Android tidak dapat dibuka.") );
		break;
	}
#else
	Q_UNUSED(path)
#endif
}



void AppUpdater::setState( const QString& state, const QString& error )
{
	m_state = state;
	m_error = error;
	Q_EMIT changed();
}
