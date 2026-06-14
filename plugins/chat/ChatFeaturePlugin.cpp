/*
 * ChatFeaturePlugin.cpp - implementation of ChatFeaturePlugin
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

#include "ChatFeaturePlugin.h"
#include "ChatWidget.h"
#include "ComputerControlInterface.h"
#include "FeatureWorkerManager.h"
#include "VeyonServerInterface.h"
#include "VeyonWorkerInterface.h"


ChatFeaturePlugin::ChatFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_chatFeature( Feature( QStringLiteral( "Chat" ),
							Feature::Flag::Action | Feature::Flag::AllComponents,
							Feature::Uid( "8c3a9e21-5b4d-47f6-a1c8-0d2e6f9b3a74" ),
							Feature::Uid(),
							tr( "Chat" ), {},
							tr( "Use this function to chat with the users of selected computers." ),
							QStringLiteral(":/chat/chat.png") ) ),
	m_features( { m_chatFeature } )
{
}



ChatFeaturePlugin::~ChatFeaturePlugin()
{
	for( const auto& chat : std::as_const( m_masterChats ) )
	{
		delete chat;
	}
	delete m_workerChat;
}



const FeatureList& ChatFeaturePlugin::featureList() const
{
	return m_features;
}



bool ChatFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation,
										const QVariantMap& arguments,
										const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(operation)
	Q_UNUSED(arguments)
	Q_UNUSED(computerControlInterfaces)

	Q_UNUSED(featureUid)
	// the chat is driven interactively through the chat windows, not via
	// scripted control operations
	return false;
}



ChatWidget* ChatFeaturePlugin::masterChatFor( const ComputerControlInterface::Pointer& controlInterface )
{
	auto chat = m_masterChats.value( controlInterface.data() ).data();
	if( chat == nullptr )
	{
		chat = new ChatWidget( tr( "Chat – %1" ).arg( controlInterface->computer().displayName() ) );
		m_masterChats[controlInterface.data()] = chat;

		const auto controlInterfaceCopy = controlInterface;
		connect( chat, &ChatWidget::messageEntered, this,
				 [this, controlInterfaceCopy]( const QString& text ) {
			if( auto window = m_masterChats.value( controlInterfaceCopy.data() ).data() )
			{
				window->appendMessage( tr( "Me" ), text, true );
			}
			sendFeatureMessage( FeatureMessage{ m_chatFeature.uid(), ChatMessage }
									.addArgument( Argument::Text, text )
									.addArgument( Argument::Sender, tr( "Teacher" ) ),
								{ controlInterfaceCopy } );
		} );
	}
	return chat;
}



bool ChatFeaturePlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
									  const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(master)

	if( feature.uid() != m_chatFeature.uid() )
	{
		return false;
	}

	for( const auto& controlInterface : computerControlInterfaces )
	{
		auto chat = masterChatFor( controlInterface );
		chat->show();
		chat->raise();
		chat->activateWindow();
	}

	return true;
}



bool ChatFeaturePlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
											  const FeatureMessage& message )
{
	if( message.featureUid() != m_chatFeature.uid() )
	{
		return false;
	}

	// a reply coming back from a client - show it in that computer's chat window
	auto chat = masterChatFor( computerControlInterface );
	chat->appendMessage( message.argument( Argument::Sender ).toString(),
						 message.argument( Argument::Text ).toString() );
	chat->show();
	chat->raise();
	return true;
}



bool ChatFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
											  const FeatureMessage& message )
{
	if( message.featureUid() != m_chatFeature.uid() )
	{
		return false;
	}

	// remember which master sent this so worker replies can be relayed back
	m_masterContext = messageContext;
	server.featureWorkerManager().sendMessageToUnmanagedSessionWorker( message );
	return true;
}



bool ChatFeaturePlugin::handleFeatureMessageFromWorker( VeyonServerInterface& server,
														const FeatureMessage& message )
{
	if( message.featureUid() != m_chatFeature.uid() )
	{
		return false;
	}

	// relay the user's reply back to the master
	return server.sendFeatureMessageReply( m_masterContext, message );
}



bool ChatFeaturePlugin::handleFeatureMessage( VeyonWorkerInterface& worker, const FeatureMessage& message )
{
	if( message.featureUid() != m_chatFeature.uid() )
	{
		return false;
	}

	m_worker = &worker;

	if( m_workerChat == nullptr )
	{
		m_workerChat = new ChatWidget( tr( "Chat with teacher" ) );
		connect( m_workerChat, &ChatWidget::messageEntered, this, [this]( const QString& text ) {
			if( m_workerChat )
			{
				m_workerChat->appendMessage( tr( "Me" ), text, true );
			}
			if( m_worker )
			{
				m_worker->sendFeatureMessageReply( FeatureMessage{ m_chatFeature.uid(), ChatMessage }
													   .addArgument( Argument::Text, text )
													   .addArgument( Argument::Sender, tr( "Student" ) ) );
			}
		} );
	}

	m_workerChat->appendMessage( message.argument( Argument::Sender ).toString(),
								 message.argument( Argument::Text ).toString() );
	m_workerChat->show();
	m_workerChat->raise();
	return true;
}
