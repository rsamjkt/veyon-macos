/*
 * ChatWidget.cpp - a simple two-way chat window (shared by master and worker)
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

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#include "ChatWidget.h"


ChatWidget::ChatWidget( const QString& title, QWidget* parent ) :
	QWidget( parent, Qt::Window ),
	m_history( new QTextEdit( this ) ),
	m_input( new QLineEdit( this ) )
{
	setWindowTitle( title );
	resize( 420, 360 );

	m_history->setReadOnly( true );

	m_input->setPlaceholderText( tr( "Type a message and press Enter…" ) );

	auto sendButton = new QPushButton( tr( "Send" ), this );

	auto inputLayout = new QHBoxLayout;
	inputLayout->addWidget( m_input );
	inputLayout->addWidget( sendButton );

	auto layout = new QVBoxLayout( this );
	layout->addWidget( m_history );
	layout->addLayout( inputLayout );

	connect( m_input, &QLineEdit::returnPressed, this, &ChatWidget::sendCurrentInput );
	connect( sendButton, &QPushButton::clicked, this, &ChatWidget::sendCurrentInput );
}



void ChatWidget::appendMessage( const QString& sender, const QString& text, bool ownMessage )
{
	const auto color = ownMessage ? QStringLiteral("#1769aa") : QStringLiteral("#c25c1f");
	m_history->append( QStringLiteral("<b style=\"color:%1\">%2:</b> %3")
						   .arg( color, sender.toHtmlEscaped(), text.toHtmlEscaped() ) );
}



void ChatWidget::sendCurrentInput()
{
	const auto text = m_input->text().trimmed();
	if( text.isEmpty() )
	{
		return;
	}
	m_input->clear();
	Q_EMIT messageEntered( text );
}
