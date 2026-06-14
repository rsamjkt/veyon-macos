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
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "ApplicationListDialog.h"


ApplicationListDialog::ApplicationListDialog( const QString& computerName, QWidget* parent ) :
	QWidget( parent, Qt::Window ),
	m_frontmost( new QLabel( this ) ),
	m_list( new QListWidget( this ) )
{
	m_computerName = computerName;
	setWindowTitle( tr( "Applications – %1" ).arg( computerName ) );
	resize( 320, 460 );

	m_frontmost->setTextFormat( Qt::RichText );
	m_frontmost->setText( tr( "Active application: <i>(querying…)</i>" ) );

	auto closeButton = new QPushButton( tr( "Close selected application" ), this );
	closeButton->setEnabled( false );

	auto layout = new QVBoxLayout( this );
	layout->addWidget( m_frontmost );
	layout->addWidget( m_list );
	layout->addWidget( closeButton );

	connect( m_list, &QListWidget::itemSelectionChanged, this, [this, closeButton]() {
		closeButton->setEnabled( m_list->currentItem() != nullptr );
	} );

	connect( closeButton, &QPushButton::clicked, this, [this]() {
		auto item = m_list->currentItem();
		if( item == nullptr )
		{
			return;
		}
		const auto application = item->text();
		if( QMessageBox::question( this, tr( "Close application" ),
								   tr( "Close \"%1\" on %2?" ).arg( application, m_computerName ) )
			== QMessageBox::Yes )
		{
			Q_EMIT terminateRequested( application );
		}
	} );

	// poll the computer for its running applications
	auto timer = new QTimer( this );
	connect( timer, &QTimer::timeout, this, &ApplicationListDialog::refreshRequested );
	timer->start( 3000 );
}



void ApplicationListDialog::setApplications( const QStringList& applications, const QString& frontmost )
{
	m_frontmost->setText( tr( "Active application: <b style=\"color:#c25c1f\">%1</b>" )
							  .arg( frontmost.toHtmlEscaped() ) );

	// preserve the user's current selection across the 3-second auto-refresh
	const auto selected = m_list->currentItem() ? m_list->currentItem()->text() : QString();

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
		if( application == selected )
		{
			m_list->setCurrentItem( item );
		}
	}
}
