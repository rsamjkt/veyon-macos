/*
 * Notifier.cpp - sends alerts of the gateway to the admin's phone (Telegram)
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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QRegularExpression>

#include "Notifier.h"
#include "VeyonCore.h"


namespace {

QUrl apiUrl( const QString& botToken, const QString& method )
{
	return QUrl( QStringLiteral("https://api.telegram.org/bot%1/%2").arg( botToken, method ) );
}



QString replyError( QNetworkReply* reply, const QJsonObject& json )
{
	const auto description = json[QStringLiteral("description")].toString();
	if( description.isEmpty() == false )
	{
		return description;
	}
	return reply->errorString();
}

}



Notifier::Notifier( QObject* parent ) :
	QObject( parent )
{
	m_network.setTransferTimeout( 15000 );
}



bool Notifier::isValidToken( const QString& botToken )
{
	static const QRegularExpression tokenRX{ QStringLiteral("^\\d{5,}:[A-Za-z0-9_-]{30,}$") };
	return tokenRX.match( botToken ).hasMatch();
}



void Notifier::send( const QString& botToken, const QString& chatId, const QString& text, const ResultCallback& callback )
{
	if( isValidToken( botToken ) == false || chatId.isEmpty() )
	{
		if( callback )
		{
			callback( false, tr( "Bot token or chat ID missing" ) );
		}
		return;
	}

	QNetworkRequest request( apiUrl( botToken, QStringLiteral("sendMessage") ) );
	request.setHeader( QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json") );

	const auto body = QJsonDocument( QJsonObject{
		{ QStringLiteral("chat_id"), chatId },
		{ QStringLiteral("text"), text },
		{ QStringLiteral("disable_web_page_preview"), true },
	} ).toJson( QJsonDocument::Compact );

	auto reply = m_network.post( request, body );
	connect( reply, &QNetworkReply::finished, this, [reply, callback]() {
		reply->deleteLater();
		const auto json = QJsonDocument::fromJson( reply->readAll() ).object();
		const bool ok = json[QStringLiteral("ok")].toBool();
		if( ok == false )
		{
			vWarning() << "Aruni Gateway: Telegram notification failed:" << replyError( reply, json );
		}
		if( callback )
		{
			callback( ok, ok ? QString{} : replyError( reply, json ) );
		}
	} );
}



void Notifier::detectChatId( const QString& botToken, const std::function<void( const QString&, const QString& )>& callback )
{
	if( isValidToken( botToken ) == false )
	{
		callback( {}, tr( "Invalid bot token" ) );
		return;
	}

	auto reply = m_network.get( QNetworkRequest( apiUrl( botToken, QStringLiteral("getUpdates") ) ) );
	connect( reply, &QNetworkReply::finished, this, [reply, callback]() {
		reply->deleteLater();
		const auto json = QJsonDocument::fromJson( reply->readAll() ).object();
		if( json[QStringLiteral("ok")].toBool() == false )
		{
			callback( {}, replyError( reply, json ) );
			return;
		}

		const auto updates = json[QStringLiteral("result")].toArray();
		for( auto i = updates.size() - 1; i >= 0; --i )
		{
			const auto update = updates.at( i ).toObject();
			const auto message = update[QStringLiteral("message")].toObject().isEmpty()
									 ? update[QStringLiteral("channel_post")].toObject()
									 : update[QStringLiteral("message")].toObject();
			const auto chat = message[QStringLiteral("chat")].toObject();
			if( chat.contains( QStringLiteral("id") ) )
			{
				callback( QString::number( chat[QStringLiteral("id")].toInteger() ), {} );
				return;
			}
		}

		callback( {}, tr( "No message found - send \"/start\" to your bot in Telegram first" ) );
	} );
}
