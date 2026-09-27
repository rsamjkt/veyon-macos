/*
 * AccessLogDialog.cpp - access log of the selected computers
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

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextStream>
#include <QVBoxLayout>

#include "AccessLogDialog.h"
#include "AccessLogFeaturePlugin.h"


AccessLogDialog::AccessLogDialog( AccessLogFeaturePlugin* plugin, const ComputerControlInterfaceList& computers,
								  QWidget* parent ) :
	QDialog( parent ),
	m_multiple( computers.size() > 1 )
{
	setWindowTitle( computers.size() == 1 ? tr( "Access log - %1" ).arg( computers.first()->computer().displayName() )
										  : tr( "Access log - %1 computers" ).arg( computers.size() ) );
	resize( 820, 560 );

	auto layout = new QVBoxLayout( this );
	layout->addWidget( new QLabel( tr( "Who accessed the computer, when and with which functions (newest first)." ) ) );
	m_hideGateway = new QCheckBox( tr( "Hide the automatic visits of the Aruni Gateway (screenshots, log collection)" ) );
	m_hideGateway->setChecked( true );
	layout->addWidget( m_hideGateway );
	connect( m_hideGateway, &QCheckBox::toggled, this, &AccessLogDialog::rebuildTable );

	m_table = new QTableWidget( 0, m_multiple ? 5 : 4 );
	QStringList headers{ tr( "Time" ), tr( "Event" ), tr( "From" ), tr( "User" ) };
	if( m_multiple )
	{
		headers.prepend( tr( "Computer" ) );
	}
	m_table->setHorizontalHeaderLabels( headers );
	m_table->horizontalHeader()->setSectionResizeMode( QHeaderView::ResizeToContents );
	m_table->horizontalHeader()->setStretchLastSection( true );
	m_table->verticalHeader()->hide();
	m_table->setEditTriggers( QAbstractItemView::NoEditTriggers );
	m_table->setSelectionBehavior( QAbstractItemView::SelectRows );
	m_table->setSortingEnabled( true );
	layout->addWidget( m_table, 1 );

	auto buttons = new QDialogButtonBox( QDialogButtonBox::Close );
	auto exportButton = buttons->addButton( tr( "Export to Excel (CSV)…" ), QDialogButtonBox::ActionRole );
	connect( exportButton, &QPushButton::clicked, this, &AccessLogDialog::exportCsv );
	connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::close );
	layout->addWidget( buttons );

	connect( plugin, &AccessLogFeaturePlugin::entriesReceived, this, &AccessLogDialog::addEntries );
}



QString AccessLogDialog::eventText( const QJsonObject& entry )
{
	const auto event = entry[QStringLiteral("e")].toString();
	if( event == QStringLiteral("connected") )
	{
		return tr( "Connected" );
	}
	if( event == QStringLiteral("disconnected") )
	{
		const auto seconds = entry[QStringLiteral("seconds")].toInteger();
		return seconds >= 3600 ? tr( "Disconnected after %1 h %2 min" ).arg( seconds / 3600 ).arg( ( seconds % 3600 ) / 60 )
							   : tr( "Disconnected after %1 min" ).arg( qMax<qint64>( 1, seconds / 60 ) );
	}
	if( event == QStringLiteral("auth_failed") )
	{
		return tr( "Authentication failed" );
	}
	if( event == QStringLiteral("access_denied") )
	{
		return tr( "Access denied" );
	}
	if( event == QStringLiteral("feature") )
	{
		return tr( "Function: %1" ).arg( entry[QStringLiteral("feature")].toString() );
	}
	if( event == QStringLiteral("feature_denied") )
	{
		return tr( "Function not allowed for this key: %1" ).arg( entry[QStringLiteral("feature")].toString() );
	}
	return event;
}



void AccessLogDialog::addEntries( const ComputerControlInterface::Pointer& computer, const QJsonArray& entries )
{
	const auto name = computer->computer().displayName();
	for( const auto& value : entries )
	{
		m_entries.append( { name, value.toObject() } );
	}
	rebuildTable();
}



bool AccessLogDialog::isShown( const QJsonObject& entry ) const
{
	const auto event = entry[QStringLiteral("e")].toString();
	return m_hideGateway->isChecked() == false ||
		   entry[QStringLiteral("user")].toString() != QStringLiteral("Aruni Gateway") ||
		   ( event != QStringLiteral("connected") && event != QStringLiteral("disconnected") );
}



void AccessLogDialog::rebuildTable()
{
	m_table->setSortingEnabled( false );
	m_table->setRowCount( 0 );
	for( const auto& [name, entry] : std::as_const( m_entries ) )
	{
		if( isShown( entry ) == false )
		{
			continue;
		}

		const auto row = m_table->rowCount();
		m_table->insertRow( row );
		int column = 0;
		if( m_multiple )
		{
			m_table->setItem( row, column++, new QTableWidgetItem( name ) );
		}
		const auto time = QDateTime::fromString( entry[QStringLiteral("t")].toString(), Qt::ISODate ).toLocalTime();
		m_table->setItem( row, column++, new QTableWidgetItem( time.toString( QStringLiteral("yyyy-MM-dd HH:mm:ss") ) ) );
		m_table->setItem( row, column++, new QTableWidgetItem( eventText( entry ) ) );
		m_table->setItem( row, column++, new QTableWidgetItem( entry[QStringLiteral("host")].toString() ) );
		const auto keyName = entry[QStringLiteral("key")].toString();
		m_table->setItem( row, column++, new QTableWidgetItem( keyName.isEmpty() ? entry[QStringLiteral("user")].toString()
																				  : QStringLiteral("%1 (%2)").arg( entry[QStringLiteral("user")].toString(), keyName ) ) );
	}
	m_table->setSortingEnabled( true );
	m_table->sortItems( m_multiple ? 1 : 0, Qt::DescendingOrder );
}



void AccessLogDialog::exportCsv()
{
	const auto fileName = QFileDialog::getSaveFileName( this, tr( "Export access log" ),
		QDir( QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) ).filePath( tr( "access-log.csv" ) ),
		tr( "CSV files (*.csv)" ) );
	if( fileName.isEmpty() )
	{
		return;
	}

	QFile file( fileName );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) == false )
	{
		QMessageBox::critical( this, tr( "Export access log" ), tr( "Could not write %1." ).arg( fileName ) );
		return;
	}

	QTextStream stream( &file );
	stream << QChar( 0xFEFF ) << tr( "Computer" ) << ';' << tr( "Time" ) << ';' << tr( "Event" ) << ';'
		   << tr( "From" ) << ';' << tr( "User" ) << '\n';
	for( const auto& [computer, entry] : std::as_const( m_entries ) )
	{
		if( isShown( entry ) == false )
		{
			continue;
		}
		const auto time = QDateTime::fromString( entry[QStringLiteral("t")].toString(), Qt::ISODate ).toLocalTime();
		QStringList fields{ computer, time.toString( QStringLiteral("yyyy-MM-dd HH:mm:ss") ), eventText( entry ),
							entry[QStringLiteral("host")].toString(), entry[QStringLiteral("user")].toString() };
		for( auto& field : fields )
		{
			if( field.contains( QLatin1Char(';') ) || field.contains( QLatin1Char('"') ) )
			{
				field = QLatin1Char('"') + field.replace( QLatin1Char('"'), QStringLiteral("\"\"") ) + QLatin1Char('"');
			}
		}
		stream << fields.join( QLatin1Char(';') ) << '\n';
	}
}
