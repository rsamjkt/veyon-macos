/*
 * ApplicationListDialog.cpp - shows the running applications of a computer
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

#include <QFont>
#include <QLabel>
#include <QListWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "ApplicationListDialog.h"


ApplicationListDialog::ApplicationListDialog( const QString& computerName, QWidget* parent ) :
	QWidget( parent, Qt::Window ),
	m_frontmost( new QLabel( this ) ),
	m_list( new QListWidget( this ) )
{
	setWindowTitle( tr( "Applications – %1" ).arg( computerName ) );
	resize( 320, 420 );

	m_frontmost->setTextFormat( Qt::RichText );
	m_frontmost->setText( tr( "Active application: <i>(querying…)</i>" ) );

	auto layout = new QVBoxLayout( this );
	layout->addWidget( m_frontmost );
	layout->addWidget( m_list );

	// poll the computer for its running applications
	auto timer = new QTimer( this );
	connect( timer, &QTimer::timeout, this, &ApplicationListDialog::refreshRequested );
	timer->start( 3000 );
}



void ApplicationListDialog::setApplications( const QStringList& applications, const QString& frontmost )
{
	m_frontmost->setText( tr( "Active application: <b style=\"color:#c25c1f\">%1</b>" )
							  .arg( frontmost.toHtmlEscaped() ) );

	m_list->clear();
	for( const auto& application : applications )
	{
		auto item = new QListWidgetItem( application, m_list );
		if( application == frontmost )
		{
			auto font = item->font();
			font.setBold( true );
			item->setFont( font );
		}
	}
}
