/*
 * AruniVoicePlugin.cpp - implementation of AruniVoicePlugin (two-way voice)
 *
 * Copyright (c) 2026 Arunika / AruniControl
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

#include "AruniVoicePlugin.h"
#include "AudioEngine.h"
#include "BroadcastNotice.h"
#include "VoiceWidget.h"
#include "ComputerControlInterface.h"
#include "FeatureWorkerManager.h"
#include "PlatformPluginInterface.h"
#include "PlatformUserFunctions.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"


AruniVoicePlugin::AruniVoicePlugin( QObject* parent ) :
	QObject( parent ),
	m_voiceFeature( Feature( QStringLiteral( "AruniVoice" ),
							 Feature::Flag::Action | Feature::Flag::AllComponents,
							 Feature::Uid( "b9e4172c-3a86-4d09-9f5e-1c7a8d20e6b3" ),
							 Feature::Uid(),
							 tr( "Voice" ), {},
							 tr( "Use this function to talk to the user of a selected computer "
								 "(push-to-talk or open intercom)." ),
							 QStringLiteral(":/arunivoice/voice.png") ) ),
	m_broadcastFeature( Feature( QLatin1String( BroadcastFeatureName ),
								 Feature::Flag::Action | Feature::Flag::AllComponents,
								 Feature::Uid( "6d3f8a52-e17b-4c90-a4d8-0b95c2e7f316" ),
								 Feature::Uid(),
								 tr( "Voice broadcast" ), {},
								 tr( "Use this function to speak to all selected computers at once "
									 "(announcement, no audio comes back)." ),
								 QStringLiteral(":/arunivoice/voice.png") ) ),
	m_features( { m_voiceFeature, m_broadcastFeature } )
{
}



AruniVoicePlugin::~AruniVoicePlugin()
{
	for( const auto& widget : std::as_const( m_masterWidgets ) )
	{
		delete widget;
	}
	for( const auto engine : std::as_const( m_masterEngines ) )
	{
		delete engine;
	}
	delete m_workerWidget;
	delete m_workerEngine;
	delete m_broadcastWidget;
	delete m_broadcastEngine;
	delete m_broadcastPlayer;
	delete m_broadcastNotice;
}



const FeatureList& AruniVoicePlugin::featureList() const
{
	return m_features;
}



bool AruniVoicePlugin::controlFeature( Feature::Uid featureUid, Operation operation,
									   const QVariantMap& arguments,
									   const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(featureUid)
	Q_UNUSED(operation)
	Q_UNUSED(arguments)
	Q_UNUSED(computerControlInterfaces)
	// voice is driven interactively through the talk window, not via scripted ops
	return false;
}



void AruniVoicePlugin::setupMasterSession( const ComputerControlInterface::Pointer& controlInterface )
{
	if( m_masterWidgets.value( controlInterface.data() ) != nullptr )
	{
		return;
	}

	auto engine = new AudioEngine( this );
	m_masterEngines[controlInterface.data()] = engine;

	auto widget = new VoiceWidget( tr( "Voice – %1" ).arg( controlInterface->computer().displayName() ),
								   true /* intercom toggle */ );
	m_masterWidgets[controlInterface.data()] = widget;

	const auto ci = controlInterface;

	// teacher microphone -> selected computer
	connect( engine, &AudioEngine::chunkCaptured, this, [this, ci]( const QByteArray& pcm ) {
		sendFeatureMessage( FeatureMessage{ m_voiceFeature.uid(), AudioData }
								.addArgument( Argument::Audio, pcm ), { ci } );
	} );

	// push-to-talk
	connect( widget, &VoiceWidget::talkPressed, this, [engine, widget]() {
		engine->startCapture();
		widget->setStatus( tr( "Talking…" ) );
	} );
	connect( widget, &VoiceWidget::talkReleased, this, [engine, widget]() {
		if( widget->intercomEnabled() == false )
		{
			engine->stopCapture();
			widget->setStatus( tr( "Ready" ) );
		}
	} );

	// open intercom (always-on, both directions)
	connect( widget, &VoiceWidget::intercomToggled, this, [this, ci, engine, widget]( bool on ) {
		if( on )
		{
			engine->startCapture();
			widget->setStatus( tr( "Intercom on" ) );
		}
		else
		{
			engine->stopCapture();
			widget->setStatus( tr( "Ready" ) );
		}
		sendFeatureMessage( FeatureMessage{ m_voiceFeature.uid(), SetIntercom }
								.addArgument( Argument::Enabled, on ), { ci } );
	} );
}



void AruniVoicePlugin::setupBroadcastWindow()
{
	if( m_broadcastEngine == nullptr )
	{
		m_broadcastEngine = new AudioEngine( this );
		connect( m_broadcastEngine, &AudioEngine::chunkCaptured, this, &AruniVoicePlugin::sendBroadcastChunk );
	}

	if( m_broadcastWidget )
	{
		return;
	}

	auto& userFunctions = VeyonCore::platform().userFunctions();
	m_broadcastSpeaker = userFunctions.queryCurrentUserProperty( PlatformUserFunctions::UserProperty::FullName );
	if( m_broadcastSpeaker.isEmpty() )
	{
		m_broadcastSpeaker = userFunctions.queryCurrentUserProperty( PlatformUserFunctions::UserProperty::LoginName );
	}

	m_broadcastWidget = new VoiceWidget( tr( "Voice broadcast" ), true, tr( "Keep microphone open" ) );
	m_broadcastWidget->setAttribute( Qt::WA_DeleteOnClose );

	const auto engine = m_broadcastEngine;
	const auto widget = m_broadcastWidget.data();

	const auto startTalking = [this, engine, widget]() {
		engine->startCapture();
		widget->setStatus( engine->isCapturing() ?
							   tr( "Speaking to %n computer(s)…", "", int( m_broadcastTargets.size() ) ) :
							   tr( "No microphone available" ) );
	};
	const auto stopTalking = [this, engine, widget]() {
		engine->stopCapture();
		flushBroadcast();
		widget->setStatus( tr( "Ready – %n computer(s)", "", int( m_broadcastTargets.size() ) ) );
	};

	// push-to-talk
	connect( widget, &VoiceWidget::talkPressed, this, startTalking );
	connect( widget, &VoiceWidget::talkReleased, this, [widget, stopTalking]() {
		if( widget->intercomEnabled() == false )
		{
			stopTalking();
		}
	} );

	// keep the microphone open (hands-free announcement)
	connect( widget, &VoiceWidget::intercomToggled, this, [startTalking, stopTalking]( bool on ) {
		if( on )
		{
			startTalking();
		}
		else
		{
			stopTalking();
		}
	} );

	// closing the window always ends the broadcast
	connect( widget, &VoiceWidget::closed, this, [this, engine]() {
		engine->stopCapture();
		flushBroadcast();
		m_broadcastTargets.clear();
	} );
}



void AruniVoicePlugin::sendBroadcastChunk( const QByteArray& pcm )
{
	m_broadcastBuffer.append( pcm );
	if( m_broadcastBuffer.size() >= BroadcastChunkSize )
	{
		flushBroadcast();
	}
}



void AruniVoicePlugin::flushBroadcast()
{
	if( m_broadcastBuffer.isEmpty() )
	{
		return;
	}

	sendFeatureMessage( FeatureMessage{ m_broadcastFeature.uid(), AudioData }
							.addArgument( Argument::Audio, m_broadcastBuffer )
							.addArgument( Argument::Speaker, m_broadcastSpeaker ),
						m_broadcastTargets );
	m_broadcastBuffer.clear();
}



bool AruniVoicePlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
									 const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(master)

	if( feature.uid() == m_broadcastFeature.uid() )
	{
		// one window for all computers; starting again updates the audience
		flushBroadcast();
		m_broadcastTargets = computerControlInterfaces;
		setupBroadcastWindow();
		if( m_broadcastEngine->isCapturing() == false )
		{
			m_broadcastWidget->setStatus( tr( "Ready – %n computer(s)", "", int( m_broadcastTargets.size() ) ) );
		}
		m_broadcastWidget->show();
		m_broadcastWidget->raise();
		m_broadcastWidget->activateWindow();
		return true;
	}

	if( feature.uid() != m_voiceFeature.uid() )
	{
		return false;
	}

	for( const auto& controlInterface : computerControlInterfaces )
	{
		setupMasterSession( controlInterface );
		if( auto widget = m_masterWidgets.value( controlInterface.data() ).data() )
		{
			widget->show();
			widget->raise();
			widget->activateWindow();
		}
	}

	return true;
}



bool AruniVoicePlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
											 const FeatureMessage& message )
{
	if( message.featureUid() != m_voiceFeature.uid() )
	{
		return false;
	}

	// audio coming back from the client -> play it on the teacher's speakers
	if( static_cast<int>( message.command() ) == AudioData )
	{
		if( auto engine = m_masterEngines.value( computerControlInterface.data() ) )
		{
			engine->playChunk( message.argument( Argument::Audio ).toByteArray() );
		}
	}
	return true;
}



bool AruniVoicePlugin::handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
											 const FeatureMessage& message )
{
	if( message.featureUid() == m_broadcastFeature.uid() )
	{
		// play in the user's session; the audio is live, so chunks for a session
		// that isn't there (yet) are dropped instead of queued for later - and
		// starting the worker is only retried every few seconds
		auto& workerManager = server.featureWorkerManager();
		if( workerManager.isWorkerRunning( message.featureUid() ) == false )
		{
			if( m_broadcastWorkerStartAttempt.isValid() && m_broadcastWorkerStartAttempt.elapsed() < 5000 )
			{
				return true;
			}
			m_broadcastWorkerStartAttempt.start();
			if( workerManager.startUnmanagedSessionWorker( message.featureUid() ) == false )
			{
				vDebug() << "AruniVoice broadcast: no user session to play the audio in";
				return true;
			}
		}
		workerManager.sendMessageToUnmanagedSessionWorker( message );
		return true;
	}

	if( message.featureUid() != m_voiceFeature.uid() )
	{
		return false;
	}

	// remember the master so worker audio can be relayed back, then hand the
	// message to the worker running in the user's GUI session
	m_masterContext = messageContext;
	server.featureWorkerManager().sendMessageToUnmanagedSessionWorker( message );
	return true;
}



bool AruniVoicePlugin::handleFeatureMessageFromWorker( VeyonServerInterface& server,
													   const FeatureMessage& message )
{
	if( message.featureUid() == m_broadcastFeature.uid() )
	{
		return true; // broadcasts are one-way
	}

	if( message.featureUid() != m_voiceFeature.uid() )
	{
		return false;
	}

	return server.sendFeatureMessageReply( m_masterContext, message );
}



bool AruniVoicePlugin::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
	if( message.featureUid() == m_broadcastFeature.uid() )
	{
		return handleBroadcastOnWorker( message );
	}

	if( message.featureUid() != m_voiceFeature.uid() )
	{
		return false;
	}

	m_worker = &worker;

	if( m_workerEngine == nullptr )
	{
		m_workerEngine = new AudioEngine( this );
		connect( m_workerEngine, &AudioEngine::chunkCaptured, this, [this]( const QByteArray& pcm ) {
			if( m_worker )
			{
				m_worker->sendFeatureMessageReply( FeatureMessage{ m_voiceFeature.uid(), AudioData }
													   .addArgument( Argument::Audio, pcm ) );
			}
		} );
	}

	if( m_workerWidget == nullptr )
	{
		m_workerWidget = new VoiceWidget( tr( "Talk with teacher" ), false /* no intercom toggle */ );
		connect( m_workerWidget, &VoiceWidget::talkPressed, this, [this]() {
			if( m_workerEngine ) { m_workerEngine->startCapture(); }
			if( m_workerWidget ) { m_workerWidget->setStatus( tr( "Talking…" ) ); }
		} );
		connect( m_workerWidget, &VoiceWidget::talkReleased, this, [this]() {
			if( m_workerEngine ) { m_workerEngine->stopCapture(); }
			if( m_workerWidget ) { m_workerWidget->setStatus( tr( "Ready" ) ); }
		} );
	}

	const auto command = static_cast<int>( message.command() );

	if( command == AudioData )
	{
		m_workerEngine->playChunk( message.argument( Argument::Audio ).toByteArray() );
		m_workerWidget->show();
		m_workerWidget->raise();
	}
	else if( command == SetIntercom )
	{
		if( message.argument( Argument::Enabled ).toBool() )
		{
			m_workerEngine->startCapture();   // open intercom: stream continuously
			m_workerWidget->setStatus( tr( "Intercom on" ) );
		}
		else
		{
			m_workerEngine->stopCapture();
			m_workerWidget->setStatus( tr( "Ready" ) );
		}
		m_workerWidget->show();
	}

	return true;
}



bool AruniVoicePlugin::handleBroadcastOnWorker( const FeatureMessage& message )
{
	if( message.command<Command>() != AudioData )
	{
		return true;
	}

	// playback only - the computer's microphone is never opened for broadcasts
	if( m_broadcastPlayer == nullptr )
	{
		m_broadcastPlayer = new AudioEngine( this );
	}
	m_broadcastPlayer->playChunk( message.argument( Argument::Audio ).toByteArray() );

	if( m_broadcastNotice == nullptr )
	{
		m_broadcastNotice = new BroadcastNotice;
	}
	m_broadcastNotice->ping( message.argument( Argument::Speaker ).toString() );

	return true;
}
