/*
 * ChatController.h - two-way text chat with one computer
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

#include <QHash>
#include <QVariantList>

#include "FeatureSession.h"

// Two-way text chat with the user of one computer (Chat plugin). Keeps a
// conversation per computer for the app's lifetime and counts replies that
// arrive while that conversation is not on screen.
class ChatController : public FeatureSession
{
	Q_OBJECT
	Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
	Q_PROPERTY(int unreadTotal READ unreadTotal NOTIFY unreadChanged)
	Q_PROPERTY(QString partnerName READ partnerName NOTIFY computerChanged)
public:
	explicit ChatController( ComputerGridModel* computers, QObject* parent = nullptr );

	QVariantList messages() const
	{
		return m_conversations.value( computerUid() );
	}

	int unreadTotal() const;
	QString partnerName() const;

	Q_INVOKABLE void open( const QString& uid );
	Q_INVOKABLE void close();
	Q_INVOKABLE bool send( const QString& text );
	Q_INVOKABLE int unreadCount( const QString& uid ) const
	{
		return m_unread.value( uid );
	}
	// computer with the oldest unanswered reply (for the in-app badge)
	Q_INVOKABLE QString nextUnreadUid() const;

Q_SIGNALS:
	void messagesChanged();
	void unreadChanged();
	// a reply arrived for a conversation that is not open
	void replyReceived( const QString& uid, const QString& computerName, const QString& sender, const QString& text );

protected:
	void handleMessage( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message ) override;

private:
	void append( const QString& uid, const QString& sender, const QString& text, bool own );

	QHash<QString, QVariantList> m_conversations;
	QHash<QString, int> m_unread;
	QStringList m_unreadOrder;

};
