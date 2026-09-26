/*
 * ChatController.cpp - two-way text chat with one computer
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

#include "ChatController.h"
#include "ChatFeaturePlugin.h"
#include "ComputerGridModel.h"


ChatController::ChatController( ComputerGridModel* computers, QObject* parent ) :
	FeatureSession( QStringLiteral("Chat"), computers, parent )
{
	connect( this, &FeatureSession::computerChanged, this, &ChatController::messagesChanged );
}



int ChatController::unreadTotal() const
{
	int total = 0;
	for( const auto count : m_unread )
	{
		total += count;
	}
	return total;
}



QString ChatController::partnerName() const
{
	const auto controlInterface = this->controlInterface();
	if( controlInterface.isNull() )
	{
		return {};
	}
	return controlInterface->userFullName().isEmpty() ? controlInterface->userLoginName()
													  : controlInterface->userFullName();
}



void ChatController::open( const QString& uid )
{
	setComputer( uid );

	if( m_unread.remove( uid ) > 0 )
	{
		m_unreadOrder.removeAll( uid );
		Q_EMIT unreadChanged();
	}
}



void ChatController::close()
{
	setComputer( {} );
}



bool ChatController::send( const QString& text )
{
	const auto message = text.trimmed();
	if( message.isEmpty() || isAvailable() == false )
	{
		return false;
	}

	// the sender name is shown to the user in the computer's chat window
	if( sendToComputer( FeatureMessage{ featureUid(), ChatFeaturePlugin::ChatMessage }
							.addArgument( ChatFeaturePlugin::Argument::Text, message )
							.addArgument( ChatFeaturePlugin::Argument::Sender, tr("Guru") ) ) == false )
	{
		return false;
	}

	append( computerUid(), tr("Saya"), message, true );
	return true;
}



QString ChatController::nextUnreadUid() const
{
	return m_unreadOrder.value( 0 );
}



void ChatController::handleMessage( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message )
{
	if( message.command<ChatFeaturePlugin::Command>() != ChatFeaturePlugin::ChatMessage )
	{
		return;
	}

	const auto uid = uidOf( controlInterface );
	const auto text = message.argument( ChatFeaturePlugin::Argument::Text ).toString();

	// prefer the name of the logged-in user over the generic "Student"
	auto sender = controlInterface->userFullName();
	if( sender.isEmpty() )
	{
		sender = controlInterface->userLoginName();
	}
	if( sender.isEmpty() )
	{
		sender = message.argument( ChatFeaturePlugin::Argument::Sender ).toString();
	}

	append( uid, sender, text, false );

	if( uid != computerUid() )
	{
		m_unread[uid] += 1;
		if( m_unreadOrder.contains( uid ) == false )
		{
			m_unreadOrder.append( uid );
		}
		Q_EMIT unreadChanged();
		Q_EMIT replyReceived( uid, controlInterface->computerName(), sender, text );
	}
}



void ChatController::append( const QString& uid, const QString& sender, const QString& text, bool own )
{
	m_conversations[uid].append( QVariantMap{
		{ QStringLiteral("sender"), sender },
		{ QStringLiteral("text"), text },
		{ QStringLiteral("own"), own },
		{ QStringLiteral("time"), QDateTime::currentDateTime().toString( QStringLiteral("HH:mm") ) },
	} );

	if( uid == computerUid() )
	{
		Q_EMIT messagesChanged();
	}
}
