/*
 * BroadcastController.cpp - voice broadcast (announcement) to many computers
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
#include <QPermissions>

#include "AruniVoicePlugin.h"
#include "AudioEngine.h"
#include "BroadcastController.h"
#include "ComputerGridModel.h"
#include "VoiceController.h"


BroadcastController::BroadcastController( ComputerGridModel* computers, QObject* parent ) :
	FeatureSession( QLatin1String( AruniVoicePlugin::BroadcastFeatureName ), computers, parent )
{
	// the level meter falls back smoothly between audio chunks
	m_levelTimer.setInterval( 80 );
	connect( &m_levelTimer, &QTimer::timeout, this, [this]() {
		if( m_micLevel > 0 )
		{
			const auto level = m_micLevel * 0.7;
			m_micLevel = level < 0.01 ? 0 : level;
			Q_EMIT levelChanged();
		}
	} );

	// keep the "x of y online" figure current while the sheet is open
	m_onlineTimer.setInterval( 2000 );
	connect( &m_onlineTimer, &QTimer::timeout, this, &BroadcastController::targetsChanged );
}



BroadcastController::~BroadcastController()
{
	delete m_engine;
}



int BroadcastController::targetCount() const
{
	return m_active ? int( targets().size() ) : 0;
}



int BroadcastController::onlineCount() const
{
	if( m_active == false )
	{
		return 0;
	}

	int count = 0;
	for( const auto& controlInterface : targets() )
	{
		if( controlInterface->state() == ComputerControlInterface::State::Connected )
		{
			++count;
		}
	}
	return count;
}



QString BroadcastController::permission() const
{
	switch( qApp->checkPermission( QMicrophonePermission{} ) )
	{
	case Qt::PermissionStatus::Granted: return QStringLiteral("granted");
	case Qt::PermissionStatus::Denied: return QStringLiteral("denied");
	default: break;
	}
	return QStringLiteral("undetermined");
}



void BroadcastController::open( const QStringList& uids, const QString& label )
{
	end();

	if( m_engine == nullptr )
	{
		m_engine = new AudioEngine( this );
		connect( m_engine, &AudioEngine::chunkCaptured, this, &BroadcastController::sendChunk );
	}

	m_uids = uids;
	m_label = label;
	m_active = true;
	m_microphoneMissing = false;
	m_levelTimer.start();
	m_onlineTimer.start();

	Q_EMIT targetsChanged();
	Q_EMIT stateChanged();
	Q_EMIT permissionChanged();
}



void BroadcastController::end()
{
	if( m_active == false )
	{
		return;
	}

	stopCapture();
	m_talking = false;
	m_keepOpen = false;
	m_active = false;
	m_uids.clear();
	m_audience.clear();
	m_levelTimer.stop();
	m_onlineTimer.stop();
	m_micLevel = 0;

	Q_EMIT targetsChanged();
	Q_EMIT stateChanged();
	Q_EMIT levelChanged();
}



void BroadcastController::startTalking()
{
	if( m_active == false || m_talking )
	{
		return;
	}

	if( startCapture() )
	{
		m_talking = true;
		Q_EMIT stateChanged();
	}
}



void BroadcastController::stopTalking()
{
	if( m_talking && m_keepOpen == false )
	{
		stopCapture();
		m_talking = false;
		Q_EMIT stateChanged();
	}
}



void BroadcastController::setKeepOpen( bool enabled )
{
	if( m_active == false || enabled == m_keepOpen )
	{
		return;
	}

	if( enabled )
	{
		if( m_talking == false && startCapture() == false )
		{
			return;
		}
		m_talking = true;
	}
	else
	{
		stopCapture();
		m_talking = false;
	}

	m_keepOpen = enabled;
	Q_EMIT stateChanged();
}



void BroadcastController::requestPermission()
{
	qApp->requestPermission( QMicrophonePermission{}, this, [this]( const QPermission& ) {
		Q_EMIT permissionChanged();
	} );
}



void BroadcastController::handleMessage( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message )
{
	// broadcasts are one-way
	Q_UNUSED(controlInterface)
	Q_UNUSED(message)
}



ComputerControlInterfaceList BroadcastController::targets() const
{
	return m_uids.isEmpty() ? computers()->visibleControlInterfaces() : computers()->controlInterfaces( m_uids );
}



bool BroadcastController::startCapture()
{
	if( m_engine == nullptr )
	{
		return false;
	}

	if( qApp->checkPermission( QMicrophonePermission{} ) != Qt::PermissionStatus::Granted )
	{
		requestPermission();
		return false;
	}

	m_audience = targets();
	if( onlineCount() == 0 )
	{
		Q_EMIT problem( tr("Tidak ada komputer tujuan yang terhubung.") );
		return false;
	}

	m_engine->startCapture();
	const auto missing = m_engine->isCapturing() == false;
	if( missing != m_microphoneMissing )
	{
		m_microphoneMissing = missing;
		Q_EMIT stateChanged();
	}
	if( missing )
	{
		Q_EMIT problem( tr("Mikrofon HP tidak bisa dipakai. Tutup aplikasi lain yang memakai mikrofon lalu coba lagi.") );
	}
	return missing == false;
}



void BroadcastController::stopCapture()
{
	if( m_engine )
	{
		m_engine->stopCapture();
	}
	flush();
}



void BroadcastController::sendChunk( const QByteArray& pcm )
{
	m_micLevel = qMax( m_micLevel, VoiceController::peakLevel( pcm ) );
	Q_EMIT levelChanged();

	// ~100 ms per message keeps the message rate low for large classes
	m_buffer.append( pcm );
	if( m_buffer.size() >= AruniVoicePlugin::BroadcastChunkSize )
	{
		flush();
	}
}



void BroadcastController::flush()
{
	if( m_buffer.isEmpty() )
	{
		return;
	}

	// no speaker name: the computers show their own (translated) "Teacher"
	FeatureMessage message{ featureUid(), AruniVoicePlugin::AudioData };
	message.addArgument( AruniVoicePlugin::Argument::Audio, m_buffer );
	for( const auto& controlInterface : std::as_const( m_audience ) )
	{
		send( controlInterface, message );
	}
	m_buffer.clear();
}
