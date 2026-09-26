/*
 * MonitoringCollector.cpp - screenshots and access logs of the computers
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
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUuid>

#include "ActivityLog.h"
#include "AuthenticationCredentials.h"
#include "FeatureManager.h"
#include "Filesystem.h"
#include "GatewayService.h"
#include "MonitoringCollector.h"
#include "VeyonConfiguration.h"
#include "VeyonCore.h"


namespace {

// protocol of the AccessLog feature plugin (plugins/accesslog)
const auto AccessLogFeatureUid = Feature::Uid( QStringLiteral("e81f4c27-5d3a-4b96-8c0e-3a7d9f21b640") );
constexpr int AccessLogQuery = 0;
constexpr int AccessLogEntries = 1;
constexpr int AccessLogArgumentSince = 0;
constexpr int AccessLogArgumentLimit = 1;
constexpr int AccessLogArgumentEntries = 2;

// how the collector's connections appear in the access logs of the computers
constexpr auto AnnouncedName = "Aruni Gateway";


bool hasContent( const QImage& image )
{
	if( image.isNull() )
	{
		return false;
	}

	// a 16x16 grid of samples - a real screen is never a single colour, the
	// first updates of a connection are (black framebuffer)
	const auto first = image.pixel( 0, 0 );
	int differing = 0;
	for( int y = 0; y < 16; ++y )
	{
		for( int x = 0; x < 16; ++x )
		{
			if( image.pixel( x * ( image.width() - 1 ) / 15, y * ( image.height() - 1 ) / 15 ) != first )
			{
				++differing;
			}
		}
	}
	return differing >= 8;
}



QString cursorsPath()
{
	return QDir( GatewayState::directory() ).filePath( QStringLiteral("access-cursors.json") );
}

}



MonitoringCollector::MonitoringCollector( GatewayService* service ) :
	QObject( service ),
	m_service( service )
{
	QFile file( cursorsPath() );
	if( file.open( QFile::ReadOnly ) )
	{
		m_cursors = QJsonDocument::fromJson( file.readAll() ).object();
	}

	connect( &VeyonCore::featureManager(), &FeatureManager::featureMessageReceived,
			 this, &MonitoringCollector::onFeatureMessage );

	connect( &m_timer, &QTimer::timeout, this, &MonitoringCollector::tick );
	m_timer.start( TickInterval );
}



MonitoringCollector::~MonitoringCollector()
{
	for( auto& visit : m_visits )
	{
		visit.control->stop();
	}
}



QString MonitoringCollector::screenshotDirectory()
{
	return QDir( GatewayState::directory() ).filePath( QStringLiteral("screenshots") );
}



QString MonitoringCollector::folderName( const QString& computer )
{
	static const QRegularExpression unsafe{ QStringLiteral("[^A-Za-z0-9._ -]") };
	auto name = computer;
	name.replace( unsafe, QStringLiteral("_") );
	name = name.left( 64 ).trimmed();
	return name.isEmpty() ? QStringLiteral("computer") : name;
}



QString MonitoringCollector::availableKeyName( const QString& preferred )
{
	if( preferred.isEmpty() == false && VeyonCore::isAuthenticationKeyNameValid( preferred ) &&
		QFileInfo::exists( VeyonCore::filesystem().privateKeyPath( preferred ) ) )
	{
		return preferred;
	}

	const auto baseDir = VeyonCore::filesystem().expandPath( VeyonCore::config().privateKeyBaseDir() );
	const auto names = QDir( baseDir ).entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name );
	for( const auto& name : names )
	{
		if( QFileInfo::exists( VeyonCore::filesystem().privateKeyPath( name ) ) )
		{
			return name;
		}
	}

	return {};
}



bool MonitoringCollector::loadCredentials()
{
	// veyon-server never authenticates to other computers itself, so the
	// process-wide credentials are free for the gateway's own connections
	const auto keyName = availableKeyName( m_service->state().sharedKeyName );
	if( keyName.isEmpty() )
	{
		return false;
	}

	if( keyName != m_loadedKey )
	{
		auto& credentials = VeyonCore::authenticationCredentials();
		if( credentials.loadPrivateKey( VeyonCore::filesystem().privateKeyPath( keyName ) ) == false )
		{
			vWarning() << "Aruni Gateway: cannot load private key" << keyName;
			return false;
		}
		credentials.setAuthenticationKeyName( keyName );
		credentials.setAnnouncedUsername( QString::fromLatin1( AnnouncedName ) );
		m_loadedKey = keyName;
	}

	return true;
}



void MonitoringCollector::tick()
{
	const auto& state = m_service->state();
	if( state.enabled == false || ( state.screenshotInterval <= 0 && state.collectAccessLogs == false ) )
	{
		return;
	}

	prune();

	const auto now = QDateTime::currentDateTimeUtc();
	const auto targets = m_service->collectionTargets( state.screenshotAllComputers || state.collectAccessLogs );

	bool credentialsChecked = false;
	for( const auto& target : targets )
	{
		if( m_visits.size() >= MaxParallelVisits )
		{
			break;
		}
		if( m_visits.contains( target.key ) )
		{
			continue;
		}

		const bool screenshotWanted = state.screenshotInterval > 0 &&
			( target.roaming ? ( target.local == false || state.screenshotInOffice ) : state.screenshotAllComputers );
		const auto lastShot = m_lastScreenshot.value( target.key );
		const bool screenshotDue = screenshotWanted &&
			( lastShot.isValid() == false || lastShot.secsTo( now ) >= state.screenshotInterval * 60 - 30 );

		const auto lastLog = m_lastLog.value( target.key );
		const bool logDue = state.collectAccessLogs &&
			( lastLog.isValid() == false || lastLog.secsTo( now ) >= LogInterval );

		if( screenshotDue == false && logDue == false )
		{
			continue;
		}

		if( credentialsChecked == false )
		{
			if( loadCredentials() == false )
			{
				m_service->setScreenshotError( tr( "No private authentication key on this computer" ) );
				return;
			}
			credentialsChecked = true;
		}

		if( screenshotDue )
		{
			m_lastScreenshot[target.key] = now;
		}
		if( logDue )
		{
			m_lastLog[target.key] = now;
		}
		visit( target, screenshotDue, logDue );
	}
}



void MonitoringCollector::visit( const Target& target, bool screenshot, bool log )
{
	const Computer computer( QUuid::createUuid(), target.name, target.host );
	auto control = ComputerControlInterface::Pointer::create( computer, target.port );

	Visit visit;
	visit.control = control;
	visit.target = target;
	visit.wantScreenshot = screenshot;
	visit.wantLog = log;
	visit.timeout = new QTimer( this );
	visit.timeout->setSingleShot( true );
	const auto key = target.key;
	connect( visit.timeout, &QTimer::timeout, this, [this, key]() { finishVisit( key ); } );
	visit.timeout->start( VisitTimeout );

	connect( control.data(), &ComputerControlInterface::framebufferUpdated, this, [this, key]() {
		auto it = m_visits.find( key );
		if( it != m_visits.end() && it->wantScreenshot && hasContent( it->control->framebuffer() ) )
		{
			saveScreenshot( it->target, it->control->framebuffer() );
			it->wantScreenshot = false;
			finishPart( key );
		}
	} );

	connect( control.data(), &ComputerControlInterface::stateChanged, this, [this, key]() {
		auto it = m_visits.find( key );
		if( it == m_visits.end() )
		{
			return;
		}
		const auto state = it->control->state();
		if( state == ComputerControlInterface::State::AuthenticationFailed )
		{
			m_service->setScreenshotError( tr( "The computer \"%1\" refused the key \"%2\"" ).arg( it->target.name, m_loadedKey ) );
			finishVisit( key );
		}
		else if( state == ComputerControlInterface::State::Connected && it->wantLog )
		{
			// only what is new since the last collection
			FeatureMessage query( AccessLogFeatureUid, FeatureMessage::Command( AccessLogQuery ) );
			query.addArgument( AccessLogArgumentSince, logCursor( key ) );
			query.addArgument( AccessLogArgumentLimit, 2000 );
			it->control->sendFeatureMessage( query );
		}
	} );

	m_visits.insert( key, visit );
	control->start( {}, screenshot ? ComputerControlInterface::UpdateMode::Monitoring
								   : ComputerControlInterface::UpdateMode::FeatureControlOnly );
}



void MonitoringCollector::onFeatureMessage( const ComputerControlInterface::Pointer& control, const FeatureMessage& message )
{
	if( message.featureUid() != AccessLogFeatureUid || static_cast<int>( message.command() ) != AccessLogEntries )
	{
		return;
	}

	for( auto it = m_visits.begin(); it != m_visits.end(); ++it )
	{
		if( it->control != control || it->wantLog == false )
		{
			continue;
		}

		const auto entries = QJsonDocument::fromJson( message.argument( AccessLogArgumentEntries ).toByteArray() ).array();
		QString newest;
		for( const auto& value : entries )
		{
			auto entry = value.toObject();
			const auto time = QDateTime::fromString( entry.take( QStringLiteral("t") ).toString(), Qt::ISODate );
			const auto event = entry.take( QStringLiteral("e") ).toString();
			if( time.isValid() == false || event.isEmpty() )
			{
				continue;
			}
			// the visits of this collector itself are not interesting
			if( entry[QStringLiteral("user")].toString() == QLatin1String( AnnouncedName ) &&
				( event == QStringLiteral("connected") || event == QStringLiteral("disconnected") ) )
			{
				continue;
			}
			ActivityLog::appendAt( time, QStringLiteral("access.") + event, it->target.name, entry );
			newest = time.toString( Qt::ISODate );
		}
		if( newest.isEmpty() == false )
		{
			setLogCursor( it.key(), newest );
		}

		const auto key = it.key();
		it->wantLog = false;
		finishPart( key );
		return;
	}
}



void MonitoringCollector::finishPart( const QString& key )
{
	const auto it = m_visits.constFind( key );
	if( it != m_visits.cend() && it->wantScreenshot == false && it->wantLog == false )
	{
		finishVisit( key );
	}
}



void MonitoringCollector::finishVisit( const QString& key )
{
	auto visit = m_visits.take( key );
	if( visit.control.isNull() )
	{
		return;
	}

	delete visit.timeout;
	// stopping from within one of its signals is not safe - let it finish first
	QTimer::singleShot( 0, this, [control = visit.control]() { control->stop(); } );
}



void MonitoringCollector::saveScreenshot( const Target& target, const QImage& image )
{
	const auto now = QDateTime::currentDateTime();
	const auto folder = QDir( screenshotDirectory() ).filePath( folderName( target.name ) + QLatin1Char('/') +
																now.toString( QStringLiteral("yyyy-MM-dd") ) );
	QDir().mkpath( folder );
	const auto fileName = QDir( folder ).filePath( now.toString( QStringLiteral("HH-mm-ss") ) + QStringLiteral(".jpg") );

	const auto scaled = image.width() > MaxImageWidth ? image.scaledToWidth( MaxImageWidth, Qt::SmoothTransformation ) : image;
	if( scaled.save( fileName, "JPG", 70 ) )
	{
		m_service->setScreenshotError( {} );
		// screenshots of the office computers would flood the history
		if( target.roaming )
		{
			ActivityLog::append( QStringLiteral("laptop.screenshot"), target.name,
								 { { QStringLiteral("file"), QDir( screenshotDirectory() ).relativeFilePath( fileName ) } } );
		}
	}
	else
	{
		m_service->setScreenshotError( tr( "Cannot save screenshots to %1" ).arg( folder ) );
	}
}



void MonitoringCollector::prune()
{
	const auto today = QDate::currentDate();
	if( m_lastPrune == today )
	{
		return;
	}
	m_lastPrune = today;

	const auto days = m_service->state().screenshotRetentionDays;
	if( days <= 0 )
	{
		return;
	}

	const auto oldest = today.addDays( -days );
	const QDir base( screenshotDirectory() );
	for( const auto& computer : base.entryList( QDir::Dirs | QDir::NoDotAndDotDot ) )
	{
		QDir computerDir( base.filePath( computer ) );
		for( const auto& day : computerDir.entryList( QDir::Dirs | QDir::NoDotAndDotDot ) )
		{
			const auto date = QDate::fromString( day, QStringLiteral("yyyy-MM-dd") );
			if( date.isValid() && date < oldest )
			{
				QDir( computerDir.filePath( day ) ).removeRecursively();
			}
		}
	}
}



QString MonitoringCollector::logCursor( const QString& key ) const
{
	return m_cursors.value( key ).toString();
}



void MonitoringCollector::setLogCursor( const QString& key, const QString& time )
{
	m_cursors[key] = time;
	QFile file( cursorsPath() );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) )
	{
		file.write( QJsonDocument( m_cursors ).toJson( QJsonDocument::Compact ) );
	}
}
