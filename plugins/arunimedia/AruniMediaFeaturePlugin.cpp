/*
 * AruniMediaFeaturePlugin.cpp - implementation of AruniMediaFeaturePlugin
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

#include <QProcess>

#include "AruniMediaFeaturePlugin.h"
#include "VeyonServerInterface.h"

#if defined(Q_OS_WIN)
#include "WinAudioMute.h"
#endif


AruniMediaFeaturePlugin::AruniMediaFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_muteAudioFeature( Feature( QStringLiteral( "AruniMediaMute" ),
								 Feature::Flag::Mode | Feature::Flag::AllComponents,
								 Feature::Uid( "1a7c5e93-0b62-4d18-9a4f-6e3d8c2b7f50" ),
								 Feature::Uid(),
								 tr( "Mute audio" ),
								 tr( "Unmute audio" ),
								 tr( "Use this function to mute or unmute the audio output of the "
									 "selected computers." ),
								 QStringLiteral(":/arunimedia/mute.png") ) ),
	m_features( { m_muteAudioFeature } )
{
}



const FeatureList& AruniMediaFeaturePlugin::featureList() const
{
	return m_features;
}



bool AruniMediaFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation,
											  const QVariantMap& arguments,
											  const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(arguments)

	if( featureUid != m_muteAudioFeature.uid() )
	{
		return false;
	}

	const auto command = ( operation == Operation::Start ) ? MuteAudio : UnmuteAudio;
	sendFeatureMessage( FeatureMessage{ featureUid, command }, computerControlInterfaces );
	return true;
}



bool AruniMediaFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server,
													const MessageContext& messageContext,
													const FeatureMessage& message )
{
	Q_UNUSED(server)
	Q_UNUSED(messageContext)

	if( message.featureUid() != m_muteAudioFeature.uid() )
	{
		return false;
	}

	setAudioMuted( static_cast<int>( message.command() ) == MuteAudio );
	return true;
}



void AruniMediaFeaturePlugin::setAudioMuted( bool muted )
{
	// runs in the logged-in user's session (the server is a per-user agent), so
	// this affects the user's audio output; no administrator rights are needed.
#if defined(Q_OS_WIN)
	setSystemAudioMutedWin( muted );
#else
	const auto script = muted ? QStringLiteral("set volume output muted true")
							  : QStringLiteral("set volume output muted false");
	QProcess::startDetached( QStringLiteral("/usr/bin/osascript"),
							 { QStringLiteral("-e"), script } );
#endif
}
