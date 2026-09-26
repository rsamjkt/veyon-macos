/*
 * ScreenshotScheduler.cpp - periodic screenshots of the roaming laptops
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
#include <QRegularExpression>
#include <QUuid>

#include "ActivityLog.h"
#include "AuthenticationCredentials.h"
#include "Filesystem.h"
#include "GatewayService.h"
#include "ScreenshotScheduler.h"
#include "VeyonConfiguration.h"
#include "VeyonCore.h"


namespace {

// file system safe folder name for a laptop
QString folderName( const QString& laptop )
{
	static const QRegularExpression unsafe{ QStringLiteral("[^A-Za-z0-9._ -]") };
	auto name = laptop;
	name.replace( unsafe, QStringLiteral("_") );
	return name.left( 64 ).trimmed().isEmpty() ? QStringLiteral("laptop") : name.left( 64 ).trimmed();
}



bool hasContent( const QImage& image )
{
	if( image.isNull() )
	{
		return false;
	}

	// a 16x16 grid of samples - a real screen is never a single colour
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

}



ScreenshotScheduler::ScreenshotScheduler( GatewayService* service ) :
	QObject( service ),
	m_service( service )
{
	connect( &m_timer, &QTimer::timeout, this, &ScreenshotScheduler::tick );
	m_timer.start( TickInterval );
}



ScreenshotScheduler::~ScreenshotScheduler()
{
	for( auto& capture : m_captures )
	{
		capture.interface->stop();
	}
}



QString ScreenshotScheduler::directory()
{
	return QDir( GatewayState::directory() ).filePath( QStringLiteral("screenshots") );
}



QString ScreenshotScheduler::availableKeyName( const QString& preferred )
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



bool ScreenshotScheduler::loadCredentials()
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
			vWarning() << "Aruni Gateway: cannot load private key" << keyName << "for screenshots";
			return false;
		}
		credentials.setAuthenticationKeyName( keyName );
		m_loadedKey = keyName;
	}

	return true;
}



void ScreenshotScheduler::tick()
{
	const auto& state = m_service->state();
	if( state.enabled == false || state.screenshotInterval <= 0 )
	{
		return;
	}

	prune();

	const auto now = QDateTime::currentDateTimeUtc();
	const auto laptops = m_service->onlineLaptops();
	for( const auto& laptop : laptops )
	{
		if( ( laptop.local && state.screenshotInOffice == false ) || m_captures.contains( laptop.key ) )
		{
			continue;
		}

		const auto last = m_lastCapture.value( laptop.key );
		if( last.isValid() && last.secsTo( now ) < state.screenshotInterval * 60 - 30 )
		{
			continue;
		}

		if( loadCredentials() == false )
		{
			m_service->setScreenshotError( tr( "No private authentication key on this computer" ) );
			return;
		}

		m_lastCapture[laptop.key] = now;
		capture( laptop.key, laptop.name, laptop.port );
	}
}



void ScreenshotScheduler::capture( const QByteArray& agentKey, const QString& laptop, quint16 port )
{
	const Computer computer( QUuid::createUuid(), laptop, QStringLiteral("127.0.0.1") );
	auto interface = ComputerControlInterface::Pointer::create( computer, port );

	Capture capture;
	capture.interface = interface;
	capture.laptop = laptop;
	capture.timeout = new QTimer( this );
	capture.timeout->setSingleShot( true );
	connect( capture.timeout, &QTimer::timeout, this, [this, agentKey]() {
		vWarning() << "Aruni Gateway: screenshot of" << m_captures.value( agentKey ).laptop << "timed out";
		finishCapture( agentKey, {} );
	} );
	capture.timeout->start( CaptureTimeout );

	connect( interface.data(), &ComputerControlInterface::framebufferUpdated, this, [this, agentKey]() {
		// the first updates may only carry parts of the screen (or the cursor)
		// on an all-black framebuffer - wait until there is a picture
		const auto it = m_captures.constFind( agentKey );
		if( it != m_captures.cend() && hasContent( it->interface->framebuffer() ) )
		{
			finishCapture( agentKey, it->interface->framebuffer() );
		}
	} );
	connect( interface.data(), &ComputerControlInterface::stateChanged, this, [this, agentKey]() {
		const auto it = m_captures.constFind( agentKey );
		if( it != m_captures.cend() && it->interface->state() == ComputerControlInterface::State::AuthenticationFailed )
		{
			m_service->setScreenshotError( tr( "The laptop \"%1\" refused the key \"%2\"" ).arg( it->laptop, m_loadedKey ) );
			finishCapture( agentKey, {} );
		}
	} );

	m_captures.insert( agentKey, capture );
	interface->start( {}, ComputerControlInterface::UpdateMode::Monitoring );
}



void ScreenshotScheduler::finishCapture( const QByteArray& agentKey, const QImage& image )
{
	auto capture = m_captures.take( agentKey );
	if( capture.interface.isNull() )
	{
		return;
	}

	delete capture.timeout;
	// stopping from within one of its signals is not safe - let it finish first
	QTimer::singleShot( 0, this, [interface = capture.interface]() { interface->stop(); } );

	if( image.isNull() )
	{
		return;
	}

	const auto now = QDateTime::currentDateTime();
	const auto folder = QDir( directory() ).filePath( folderName( capture.laptop ) + QLatin1Char('/') +
													  now.toString( QStringLiteral("yyyy-MM-dd") ) );
	QDir().mkpath( folder );
	const auto fileName = QDir( folder ).filePath( now.toString( QStringLiteral("HH-mm-ss") ) + QStringLiteral(".jpg") );

	const auto scaled = image.width() > MaxImageWidth ?
							image.scaledToWidth( MaxImageWidth, Qt::SmoothTransformation ) : image;
	if( scaled.save( fileName, "JPG", 70 ) )
	{
		m_service->setScreenshotError( {} );
		ActivityLog::append( QStringLiteral("laptop.screenshot"), capture.laptop,
							 { { QStringLiteral("file"), QDir( directory() ).relativeFilePath( fileName ) } } );
	}
	else
	{
		m_service->setScreenshotError( tr( "Cannot save screenshots to %1" ).arg( folder ) );
	}
}



void ScreenshotScheduler::prune()
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
	const QDir base( directory() );
	for( const auto& laptop : base.entryList( QDir::Dirs | QDir::NoDotAndDotDot ) )
	{
		QDir laptopDir( base.filePath( laptop ) );
		for( const auto& day : laptopDir.entryList( QDir::Dirs | QDir::NoDotAndDotDot ) )
		{
			const auto date = QDate::fromString( day, QStringLiteral("yyyy-MM-dd") );
			if( date.isValid() && date < oldest )
			{
				QDir( laptopDir.filePath( day ) ).removeRecursively();
			}
		}
	}
}
