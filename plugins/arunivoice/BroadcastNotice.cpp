/*
 * BroadcastNotice.cpp - unobtrusive "teacher is speaking" notice
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

#include <QGuiApplication>
#include <QPainter>
#include <QScreen>

#include "BroadcastNotice.h"


BroadcastNotice::BroadcastNotice( QWidget* parent ) :
	QLabel( parent, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
					Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput )
{
	setAttribute( Qt::WA_ShowWithoutActivating );
	setAttribute( Qt::WA_TransparentForMouseEvents );
	setAttribute( Qt::WA_TranslucentBackground );	// rounded corners
	setFocusPolicy( Qt::NoFocus );
	setAlignment( Qt::AlignCenter );
	// the rounded background is painted in paintEvent() (style sheet backgrounds
	// aren't drawn for translucent top-level windows)
	setStyleSheet( QStringLiteral( "QLabel { color:white; font-size:15px; font-weight:bold; padding:8px 18px; }" ) );

	m_hideTimer.setSingleShot( true );
	m_hideTimer.setInterval( HideDelay );
	connect( &m_hideTimer, &QTimer::timeout, this, &QWidget::hide );
}



void BroadcastNotice::ping( const QString& speaker )
{
	const auto name = speaker.isEmpty() ? tr( "Teacher" ) : speaker;
	const auto message = QStringLiteral("📢  ") + tr( "%1 is speaking" ).arg( name );
	if( message != text() )
	{
		setText( message );
		adjustSize();
	}

	if( isVisible() == false )
	{
		placeOnScreen();
		show();
	}

	m_hideTimer.start();
}



void BroadcastNotice::paintEvent( QPaintEvent* event )
{
	QPainter painter( this );
	painter.setRenderHint( QPainter::Antialiasing );
	painter.setPen( Qt::NoPen );
	painter.setBrush( QColor( 242, 129, 47, 235 ) );
	painter.drawRoundedRect( rect(), height() / 2.0, height() / 2.0 );
	painter.end();

	QLabel::paintEvent( event );
}



void BroadcastNotice::placeOnScreen()
{
	adjustSize();

	auto screen = QGuiApplication::primaryScreen();
	if( screen == nullptr )
	{
		return;
	}

	// top center, below the menu bar / above maximized windows' title bars
	const auto area = screen->availableGeometry();
	move( area.x() + ( area.width() - width() ) / 2, area.y() + 12 );
}
