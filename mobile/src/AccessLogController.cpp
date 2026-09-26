/*
 * AccessLogController.cpp - access log of one computer (AccessLog plugin)
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

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

#include "AccessLogController.h"
#include "AccessLogFeaturePlugin.h"
#include "FeatureManager.h"


AccessLogController::AccessLogController( ComputerGridModel* computers, QObject* parent ) :
	FeatureSession( Feature::Uid( AccessLogFeaturePlugin::FeatureUid ), computers, parent )
{
	// computers with an older AruniControl never answer
	m_timeoutTimer.setSingleShot( true );
	m_timeoutTimer.setInterval( 10000 );
	connect( &m_timeoutTimer, &QTimer::timeout, this, [this]() {
		if( m_state == QLatin1String("loading") )
		{
			setState( QStringLiteral("noresponse") );
		}
	} );

	connect( this, &FeatureSession::onlineChanged, this, [this]() {
		if( computerUid().isEmpty() == false && ( isOnline() || m_state != QLatin1String("ready") ) )
		{
			refresh();
		}
	} );
}



void AccessLogController::open( const QString& uid )
{
	if( uid != computerUid() )
	{
		m_entries.clear();
		m_updatedAt.clear();
		Q_EMIT entriesChanged();
		setState( {} );
	}

	setComputer( uid );
	refresh();
}



void AccessLogController::close()
{
	m_timeoutTimer.stop();
	setComputer( {} );
}



void AccessLogController::refresh()
{
	if( computerUid().isEmpty() )
	{
		return;
	}

	if( sendToComputer( FeatureMessage{ featureUid(), AccessLogFeaturePlugin::Query }
							.addArgument( AccessLogFeaturePlugin::Argument::Since, QString{} )
							.addArgument( AccessLogFeaturePlugin::Argument::Limit, Limit ) ) == false )
	{
		setState( QStringLiteral("offline") );
		return;
	}

	// a refresh keeps the current list visible
	if( m_state != QLatin1String("ready") )
	{
		setState( QStringLiteral("loading") );
	}
	m_timeoutTimer.start();
}



QString AccessLogController::formatDuration( qint64 seconds )
{
	seconds = qMax<qint64>( 0, seconds );
	const auto hours = seconds / 3600;
	const auto minutes = ( seconds % 3600 ) / 60;
	const auto rest = seconds % 60;

	if( hours > 0 )
	{
		return minutes > 0 ? tr("%1 j %2 mnt").arg( hours ).arg( minutes ) : tr("%1 jam").arg( hours );
	}
	if( minutes > 0 )
	{
		return rest > 0 ? tr("%1 mnt %2 dtk").arg( minutes ).arg( rest ) : tr("%1 menit").arg( minutes );
	}
	return tr("%1 dtk").arg( rest );
}



void AccessLogController::handleMessage( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message )
{
	if( message.command<AccessLogFeaturePlugin::Command>() != AccessLogFeaturePlugin::Entries ||
		uidOf( controlInterface ) != computerUid() )
	{
		return;
	}

	m_timeoutTimer.stop();
	setEntries( QJsonDocument::fromJson( message.argument( AccessLogFeaturePlugin::Argument::Entries ).toByteArray() ).array() );
	m_updatedAt = QDateTime::currentDateTime().toString( QStringLiteral("HH:mm:ss") );
	Q_EMIT entriesChanged();
	setState( QStringLiteral("ready") );
}



void AccessLogController::setEntries( const QJsonArray& entries )
{
	const QLocale locale( QLocale::Indonesian, QLocale::Indonesia );
	const auto today = QDate::currentDate();

	m_entries.clear();
	m_entries.reserve( entries.size() );

	// the log is oldest first - show the newest first
	for( auto i = entries.size() - 1; i >= 0; --i )
	{
		const auto entry = entries.at( i ).toObject();
		const auto time = QDateTime::fromString( entry.value( QStringLiteral("t") ).toString(), Qt::ISODate ).toLocalTime();
		const auto event = entry.value( QStringLiteral("e") ).toString();
		const auto user = entry.value( QStringLiteral("user") ).toString();
		const auto host = entry.value( QStringLiteral("host") ).toString();

		QString title;
		QString icon;
		QString kind;
		if( event == QLatin1String("connected") )
		{
			title = tr("Terhubung");
			icon = QStringLiteral("login");
			kind = QStringLiteral("ok");
		}
		else if( event == QLatin1String("disconnected") )
		{
			const auto seconds = entry.value( QStringLiteral("seconds") );
			title = seconds.isDouble() ? tr("Terputus (%1)").arg( formatDuration( qint64( seconds.toDouble() ) ) )
									   : tr("Terputus");
			icon = QStringLiteral("logout");
			kind = QStringLiteral("info");
		}
		else if( event == QLatin1String("auth_failed") )
		{
			title = tr("Autentikasi gagal");
			icon = QStringLiteral("key");
			kind = QStringLiteral("danger");
		}
		else if( event == QLatin1String("access_denied") )
		{
			title = tr("Akses ditolak");
			icon = QStringLiteral("block");
			kind = QStringLiteral("danger");
		}
		else if( event == QLatin1String("feature") )
		{
			title = tr("Fitur: %1").arg( featureLabel( entry.value( QStringLiteral("feature") ).toString() ) );
			icon = QStringLiteral("bolt");
			kind = QStringLiteral("warning");
		}
		else
		{
			title = event;
			icon = QStringLiteral("info");
			kind = QStringLiteral("info");
		}

		QString who = user;
		if( host.isEmpty() == false )
		{
			who = who.isEmpty() ? host : tr("%1 · %2").arg( who, host );
		}

		QString day;
		const auto date = time.date();
		if( date == today )
		{
			day = tr("Hari ini");
		}
		else if( date == today.addDays( -1 ) )
		{
			day = tr("Kemarin");
		}
		else
		{
			day = locale.toString( date, QStringLiteral("dddd, d MMMM yyyy") );
		}

		m_entries.append( QVariantMap{
			{ QStringLiteral("time"), time.isValid() ? time.toString( QStringLiteral("HH:mm") ) : QString{} },
			{ QStringLiteral("day"), day },
			{ QStringLiteral("title"), title },
			{ QStringLiteral("detail"), who.isEmpty() ? tr("Tidak diketahui") : who },
			{ QStringLiteral("icon"), icon },
			{ QStringLiteral("kind"), kind }
		} );
	}
}



void AccessLogController::setState( const QString& state )
{
	if( state != m_state )
	{
		m_state = state;
		Q_EMIT stateChanged();
	}
}



QString AccessLogController::featureLabel( const QString& feature )
{
	// the computer logs the (translated) display name; the English names of
	// the app's own feature list get Indonesian labels here
	static const QMap<QString, QString> labels{
		{ QStringLiteral("ScreenLock"), tr("Kunci layar") },
		{ QStringLiteral("InputDevicesLock"), tr("Kunci keyboard & mouse") },
		{ QStringLiteral("TextMessage"), tr("Kirim pesan") },
		{ QStringLiteral("RemoteView"), tr("Lihat layar") },
		{ QStringLiteral("RemoteControl"), tr("Kendali jarak jauh") },
		{ QStringLiteral("Demo"), tr("Demo") },
		{ QStringLiteral("FullScreenDemo"), tr("Demo layar penuh") },
		{ QStringLiteral("WindowDemo"), tr("Demo jendela") },
		{ QStringLiteral("ShareUserScreenFullScreen"), tr("Tampilkan layar siswa") },
		{ QStringLiteral("ShareUserScreenWindow"), tr("Tampilkan layar siswa") },
		{ QStringLiteral("PowerOn"), tr("Nyalakan") },
		{ QStringLiteral("Reboot"), tr("Mulai ulang") },
		{ QStringLiteral("PowerDown"), tr("Matikan") },
		{ QStringLiteral("PowerDownNow"), tr("Matikan") },
		{ QStringLiteral("PowerDownDelayed"), tr("Matikan (tertunda)") },
		{ QStringLiteral("UserLogin"), tr("Login pengguna") },
		{ QStringLiteral("UserLogoff"), tr("Keluarkan pengguna") },
		{ QStringLiteral("StartApp"), tr("Jalankan aplikasi") },
		{ QStringLiteral("OpenWebsite"), tr("Buka website") },
		{ QStringLiteral("DistributeFiles"), tr("Kirim file") },
		{ QStringLiteral("FileCollect"), tr("Kumpulkan file") },
		{ QStringLiteral("Screenshot"), tr("Tangkap layar") },
		{ QStringLiteral("Chat"), tr("Chat") },
		{ QStringLiteral("AruniVoice"), tr("Bicara") },
		{ QStringLiteral("AruniVoiceBroadcast"), tr("Siaran suara") },
		{ QStringLiteral("ApplicationMonitoring"), tr("Aplikasi berjalan") },
		{ QStringLiteral("InternetAccessControl"), tr("Kontrol internet") },
		{ QStringLiteral("AruniMediaMute"), tr("Bisukan suara") },
		{ QStringLiteral("SiteFilter"), tr("Blokir situs") },
	};

	for( const auto& known : VeyonCore::featureManager().features() )
	{
		if( known.name() == feature || known.displayName() == feature )
		{
			return labels.value( known.name(), known.displayName() );
		}
	}
	return labels.value( feature, feature );
}
