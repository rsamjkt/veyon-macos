/*
 * VoiceWidget.cpp - push-to-talk / intercom UI for AruniVoice
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

#include <QCheckBox>
#include <QCloseEvent>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "VoiceWidget.h"


VoiceWidget::VoiceWidget( const QString& title, bool showIntercomToggle,
						  const QString& toggleText, QWidget* parent ) :
	QWidget( parent, Qt::Window ),
	m_status( new QLabel( this ) )
{
	setWindowTitle( title );
	resize( 280, 180 );

	auto talkButton = new QPushButton( tr( "🎤  Hold to talk" ), this );
	talkButton->setMinimumHeight( 64 );
	talkButton->setStyleSheet( QStringLiteral(
		"QPushButton { font-size: 16px; font-weight: bold; border-radius: 10px;"
		" background:#F2812F; color:white; }"
		"QPushButton:pressed { background:#c25c1f; }" ) );

	m_status->setAlignment( Qt::AlignCenter );
	m_status->setText( tr( "Ready" ) );

	auto layout = new QVBoxLayout( this );
	layout->addWidget( talkButton );
	layout->addWidget( m_status );

	if( showIntercomToggle )
	{
		m_intercom = new QCheckBox( toggleText.isEmpty() ? tr( "Open intercom (always-on, use a headset)" )
														 : toggleText, this );
		layout->addWidget( m_intercom );
		connect( m_intercom, &QCheckBox::toggled, this, &VoiceWidget::intercomToggled );
		connect( m_intercom, &QCheckBox::toggled, talkButton, &QPushButton::setDisabled );
	}

	connect( talkButton, &QPushButton::pressed, this, &VoiceWidget::talkPressed );
	connect( talkButton, &QPushButton::released, this, &VoiceWidget::talkReleased );
}



void VoiceWidget::setStatus( const QString& text )
{
	m_status->setText( text );
}



bool VoiceWidget::intercomEnabled() const
{
	return m_intercom != nullptr && m_intercom->isChecked();
}



void VoiceWidget::closeEvent( QCloseEvent* event )
{
	Q_EMIT closed();
	QWidget::closeEvent( event );
}
