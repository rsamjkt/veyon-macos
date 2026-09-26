/*
 * Notifier.h - sends alerts of the gateway to the admin's phone (Telegram)
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

#pragma once

#include <QNetworkAccessManager>
#include <QObject>

#include <functional>

// Push messages through a Telegram bot the admin creates with @BotFather:
// no app store account, no push service of our own, works on every phone.
class Notifier : public QObject
{
	Q_OBJECT
public:
	using ResultCallback = std::function<void( bool ok, const QString& error )>;

	explicit Notifier( QObject* parent = nullptr );

	void send( const QString& botToken, const QString& chatId, const QString& text,
			   const ResultCallback& callback = {} );

	// the chat of the most recent message sent to the bot (the admin sends
	// "/start" to the bot first), for filling in the chat ID
	void detectChatId( const QString& botToken, const std::function<void( const QString& chatId, const QString& error )>& callback );

	static bool isValidToken( const QString& botToken );

private:
	QNetworkAccessManager m_network;

};
