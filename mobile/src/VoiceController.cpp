/*
 * VoiceController.cpp - AruniVoice push-to-talk / intercom with one computer
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
#include <QtEndian>

#include <cmath>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

#include "AruniVoicePlugin.h"
#include "AudioEngine.h"
#include "VoiceController.h"


VoiceController::VoiceController( ComputerGridModel* computers, QObject* parent ) :
	FeatureSession( QStringLiteral("AruniVoice"), computers, parent )
{
	// level meters fall back smoothly between audio chunks
	m_levelTimer.setInterval( 80 );
	connect( &m_levelTimer, &QTimer::timeout, this, &VoiceController::decayLevels );

	// the session ends when the computer goes away
	connect( this, &FeatureSession::onlineChanged, this, [this]() {
		if( isActive() && isOnline() == false && ( m_talking || m_intercom ) )
		{
			m_intercom = false;
			setTalking( false );
		}
	} );
}



VoiceController::~VoiceController()
{
	delete m_engine;
}



void VoiceController::setSpeakerMuted( bool muted )
{
	if( muted != m_speakerMuted )
	{
		m_speakerMuted = muted;
		Q_EMIT stateChanged();
	}
}



QString VoiceController::permission() const
{
	switch( qApp->checkPermission( QMicrophonePermission{} ) )
	{
	case Qt::PermissionStatus::Granted: return QStringLiteral("granted");
	case Qt::PermissionStatus::Denied: return QStringLiteral("denied");
	default: break;
	}
	return QStringLiteral("undetermined");
}



bool VoiceController::isRemoteSpeaking() const
{
	return m_lastRemoteAudio.isValid() && m_lastRemoteAudio.elapsed() < 600;
}



void VoiceController::open( const QString& uid )
{
	if( uid != computerUid() )
	{
		end();
	}

	if( m_engine == nullptr )
	{
		m_engine = new AudioEngine( this );
		connect( m_engine, &AudioEngine::chunkCaptured, this, [this]( const QByteArray& pcm ) {
			m_micLevel = qMax( m_micLevel, peakLevel( pcm ) );
			Q_EMIT levelsChanged();
			sendToComputer( FeatureMessage{ featureUid(), AruniVoicePlugin::AudioData }
								.addArgument( AruniVoicePlugin::Argument::Audio, pcm ) );
		} );
	}

	m_microphoneMissing = false;
	m_levelTimer.start();
	setComputer( uid );
	Q_EMIT stateChanged();
	Q_EMIT permissionChanged();
}



void VoiceController::end()
{
	if( isActive() == false )
	{
		return;
	}

	if( m_intercom )
	{
		sendToComputer( FeatureMessage{ featureUid(), AruniVoicePlugin::SetIntercom }
							.addArgument( AruniVoicePlugin::Argument::Enabled, false ) );
	}

	m_intercom = false;
	m_talking = false;
	stopCapture();

	// dropping the engine also stops (and flushes) the playback
	delete m_engine;

	m_levelTimer.stop();
	m_micLevel = 0;
	m_remoteLevel = 0;
	m_lastRemoteAudio.invalidate();

	setComputer( {} );
	Q_EMIT stateChanged();
	Q_EMIT levelsChanged();
}



void VoiceController::startTalking()
{
	if( isActive() == false || m_talking || m_intercom )
	{
		return;
	}

	if( isOnline() == false )
	{
		Q_EMIT problem( tr("%1 sedang tidak terhubung.").arg( computerName() ) );
		return;
	}

	if( startCapture() )
	{
		setTalking( true );
	}
}



void VoiceController::stopTalking()
{
	if( m_talking && m_intercom == false )
	{
		stopCapture();
		setTalking( false );
	}
}



void VoiceController::setIntercom( bool enabled )
{
	if( isActive() == false || enabled == m_intercom )
	{
		return;
	}

	if( enabled )
	{
		if( isOnline() == false )
		{
			Q_EMIT problem( tr("%1 sedang tidak terhubung.").arg( computerName() ) );
			return;
		}
		if( startCapture() == false )
		{
			return;
		}
	}
	else
	{
		stopCapture();
	}

	// the computer starts (or stops) streaming its microphone as well
	sendToComputer( FeatureMessage{ featureUid(), AruniVoicePlugin::SetIntercom }
						.addArgument( AruniVoicePlugin::Argument::Enabled, enabled ) );

	m_intercom = enabled;
	m_talking = enabled;
	Q_EMIT stateChanged();
}



void VoiceController::requestPermission()
{
	qApp->requestPermission( QMicrophonePermission{}, this, [this]( const QPermission& ) {
		Q_EMIT permissionChanged();
	} );
}



void VoiceController::openPermissionSettings()
{
#ifdef Q_OS_ANDROID
	const auto context = QJniObject( QNativeInterface::QAndroidApplication::context() );
	const auto packageName = context.callObjectMethod<jstring>( "getPackageName" );
	const auto uri = QJniObject::callStaticObjectMethod( "android/net/Uri", "fromParts",
														 "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Landroid/net/Uri;",
														 QJniObject::fromString( QStringLiteral("package") ).object<jstring>(),
														 packageName.object<jstring>(), nullptr );
	auto intent = QJniObject( "android/content/Intent", "(Ljava/lang/String;Landroid/net/Uri;)V",
							  QJniObject::fromString( QStringLiteral("android.settings.APPLICATION_DETAILS_SETTINGS") ).object<jstring>(),
							  uri.object() );
	intent.callObjectMethod( "addFlags", "(I)Landroid/content/Intent;", 0x10000000 ); // FLAG_ACTIVITY_NEW_TASK
	context.callMethod<void>( "startActivity", "(Landroid/content/Intent;)V", intent.object() );
#endif
}



void VoiceController::handleMessage( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message )
{
	if( message.command<AruniVoicePlugin::Command>() != AruniVoicePlugin::AudioData ||
		uidOf( controlInterface ) != computerUid() || m_engine == nullptr )
	{
		return;
	}

	const auto pcm = message.argument( AruniVoicePlugin::Argument::Audio ).toByteArray();
	m_lastRemoteAudio.start();
	m_remoteLevel = qMax( m_remoteLevel, peakLevel( pcm ) );
	Q_EMIT levelsChanged();

	if( m_speakerMuted == false )
	{
		m_engine->playChunk( pcm );
	}
}



bool VoiceController::startCapture()
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



void VoiceController::stopCapture()
{
	if( m_engine )
	{
		m_engine->stopCapture();
	}
}



void VoiceController::setTalking( bool talking )
{
	if( talking != m_talking )
	{
		m_talking = talking;
		Q_EMIT stateChanged();
	}
}



void VoiceController::decayLevels()
{
	if( m_micLevel <= 0 && m_remoteLevel <= 0 && m_lastRemoteAudio.isValid() == false )
	{
		return;
	}
	if( m_lastRemoteAudio.isValid() && m_lastRemoteAudio.elapsed() > 1000 )
	{
		m_lastRemoteAudio.invalidate();
	}

	const auto micLevel = m_micLevel * 0.7;
	const auto remoteLevel = m_remoteLevel * 0.7;
	m_micLevel = micLevel < 0.01 ? 0 : micLevel;
	m_remoteLevel = remoteLevel < 0.01 ? 0 : remoteLevel;
	Q_EMIT levelsChanged();
}



qreal VoiceController::peakLevel( const QByteArray& pcm )
{
	// 16-bit signed little-endian samples (AudioEngine's voice format)
	const auto samples = pcm.size() / 2;
	int peak = 0;
	for( qsizetype i = 0; i < samples; ++i )
	{
		const auto sample = qFromLittleEndian<qint16>( pcm.constData() + i * 2 );
		peak = qMax( peak, qAbs( int( sample ) ) );
	}

	// perceptual-ish curve so normal speech moves the meter visibly
	return qMin<qreal>( 1.0, std::sqrt( qreal( peak ) / 32768.0 ) * 1.2 );
}
