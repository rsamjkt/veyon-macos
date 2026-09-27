/*
 * InventoryView.cpp - inventory of the computers in the Configurator
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
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "GatewayState.h"
#include "InventoryView.h"


namespace {

enum InventoryColumn
{
	InvColumnName,
	InvColumnModel,
	InvColumnSystem,
	InvColumnCpu,
	InvColumnRam,
	InvColumnDisk,
	InvColumnPrograms,
	InvColumnLastSeen,
	InvColumnCount
};



QString gb( double value )
{
	return QLocale().toString( value, 'f', value < 10 ? 1 : 0 ) + QStringLiteral(" GB");
}

}



InventoryView::InventoryView( QWidget* parent ) :
	QWidget( parent )
{
	auto layout = new QVBoxLayout( this );

	auto settingsRow = new QHBoxLayout;
	m_enabled = new QCheckBox( tr( "Collect the inventory of all computers (every 6 hours)" ) );
	settingsRow->addWidget( m_enabled );
	settingsRow->addStretch( 1 );
	layout->addLayout( settingsRow );

	auto alertsRow = new QHBoxLayout;
	alertsRow->addWidget( new QLabel( tr( "Alert when a computer was not online for" ) ) );
	m_offlineDays = new QSpinBox;
	m_offlineDays->setRange( 0, 60 );
	m_offlineDays->setSuffix( tr( " days" ) );
	m_offlineDays->setSpecialValueText( tr( "never" ) );
	alertsRow->addWidget( m_offlineDays );
	alertsRow->addSpacing( 16 );
	alertsRow->addWidget( new QLabel( tr( "or a disk has less than" ) ) );
	m_diskPercent = new QSpinBox;
	m_diskPercent->setRange( 0, 50 );
	m_diskPercent->setSuffix( tr( " % free" ) );
	m_diskPercent->setSpecialValueText( tr( "never" ) );
	alertsRow->addWidget( m_diskPercent );
	alertsRow->addStretch( 1 );
	layout->addLayout( alertsRow );

	auto toolsRow = new QHBoxLayout;
	m_filter = new QLineEdit;
	m_filter->setPlaceholderText( tr( "Search computer, model, program..." ) );
	m_filter->setClearButtonEnabled( true );
	toolsRow->addWidget( m_filter, 1 );
	auto collectButton = new QPushButton( tr( "Update now" ) );
	auto exportButton = new QPushButton( tr( "Export computers..." ) );
	auto softwareButton = new QPushButton( tr( "Export software..." ) );
	toolsRow->addWidget( collectButton );
	toolsRow->addWidget( exportButton );
	toolsRow->addWidget( softwareButton );
	layout->addLayout( toolsRow );

	auto splitter = new QSplitter( Qt::Vertical );
	m_table = new QTableWidget( 0, InvColumnCount );
	m_table->setHorizontalHeaderLabels( { tr( "Computer" ), tr( "Model" ), tr( "Operating system" ), tr( "Processor" ),
										  tr( "RAM" ), tr( "Lowest free disk" ), tr( "Programs" ), tr( "Last seen" ) } );
	m_table->setSelectionBehavior( QAbstractItemView::SelectRows );
	m_table->setSelectionMode( QAbstractItemView::SingleSelection );
	m_table->setEditTriggers( QAbstractItemView::NoEditTriggers );
	m_table->setSortingEnabled( true );
	m_table->sortByColumn( InvColumnName, Qt::AscendingOrder );
	m_table->verticalHeader()->hide();
	m_table->setMinimumHeight( 160 );
	splitter->addWidget( m_table );
	m_details = new QTextBrowser;
	m_details->setMinimumHeight( 120 );
	splitter->addWidget( m_details );
	splitter->setStretchFactor( 0, 3 );
	splitter->setStretchFactor( 1, 2 );
	layout->addWidget( splitter, 1 );

	m_summary = new QLabel;
	m_summary->setWordWrap( true );
	m_summary->setStyleSheet( QStringLiteral("color: gray;") );
	layout->addWidget( m_summary );

	connect( m_enabled, &QCheckBox::toggled, this, &InventoryView::saveSettings );
	connect( m_offlineDays, &QSpinBox::valueChanged, this, &InventoryView::saveSettings );
	connect( m_diskPercent, &QSpinBox::valueChanged, this, &InventoryView::saveSettings );
	connect( m_filter, &QLineEdit::textChanged, this, &InventoryView::refresh );
	connect( m_table, &QTableWidget::itemSelectionChanged, this, &InventoryView::showDetails );
	connect( collectButton, &QPushButton::clicked, this, &InventoryView::collectNow );
	connect( exportButton, &QPushButton::clicked, this, [this]() { exportCsv( false ); } );
	connect( softwareButton, &QPushButton::clicked, this, [this]() { exportCsv( true ); } );

	loadSettings();
	refresh();
}



void InventoryView::loadSettings()
{
	const auto state = GatewayState::load();
	m_loading = true;
	m_enabled->setChecked( state.collectInventory );
	m_offlineDays->setValue( state.inventoryOfflineDays );
	m_diskPercent->setValue( state.diskAlertPercent );
	m_loading = false;
}



void InventoryView::saveSettings()
{
	if( m_loading )
	{
		return;
	}
	const auto enabled = m_enabled->isChecked();
	const auto offlineDays = m_offlineDays->value();
	const auto diskPercent = m_diskPercent->value();
	GatewayState::update( [=]( GatewayState& state ) {
		state.collectInventory = enabled;
		state.inventoryOfflineDays = offlineDays;
		state.diskAlertPercent = diskPercent;
	} );
}



void InventoryView::refresh()
{
	QString selectedKey;
	const auto selection = m_table->selectionModel()->selectedRows();
	if( selection.isEmpty() == false )
	{
		selectedKey = m_table->item( selection.first().row(), InvColumnName )->data( Qt::UserRole ).toString();
	}

	m_records = InventoryStore::list();
	const auto filter = m_filter->text().trimmed();
	const auto now = QDateTime::currentDateTimeUtc();
	const auto state = GatewayState::load();

	int shown = 0;
	int lowDisk = 0;
	int offline = 0;
	m_table->setSortingEnabled( false );
	m_table->setRowCount( 0 );
	for( const auto& record : std::as_const( m_records ) )
	{
		const auto& d = record.data;
		if( filter.isEmpty() == false )
		{
			const QString text = QString::fromUtf8( QJsonDocument( d ).toJson( QJsonDocument::Compact ) ) + record.name + record.host;
			if( text.contains( filter, Qt::CaseInsensitive ) == false )
			{
				continue;
			}
		}

		const int row = m_table->rowCount();
		m_table->insertRow( row );
		++shown;

		auto nameItem = new QTableWidgetItem( record.name );
		nameItem->setData( Qt::UserRole, record.key );
		m_table->setItem( row, InvColumnName, nameItem );
		m_table->setItem( row, InvColumnModel, new QTableWidgetItem( QStringLiteral("%1 %2").arg( d[QStringLiteral("manufacturer")].toString(),
																								  d[QStringLiteral("model")].toString() ).trimmed() ) );
		m_table->setItem( row, InvColumnSystem, new QTableWidgetItem( d[QStringLiteral("os")].toString() ) );
		m_table->setItem( row, InvColumnCpu, new QTableWidgetItem( d[QStringLiteral("cpu")].toString() ) );

		auto ramItem = new QTableWidgetItem;
		if( d.contains( QStringLiteral("ramMB") ) )
		{
			ramItem->setData( Qt::DisplayRole, QStringLiteral("%1 GB").arg( qRound( d[QStringLiteral("ramMB")].toDouble() / 1024 ) ) );
		}
		m_table->setItem( row, InvColumnRam, ramItem );

		const auto freePercent = record.lowestFreePercent();
		auto diskItem = new QTableWidgetItem( freePercent < 0 ? QString{} : QStringLiteral("%1%").arg( freePercent ) );
		if( freePercent >= 0 && state.diskAlertPercent > 0 && freePercent < state.diskAlertPercent )
		{
			diskItem->setForeground( QColor( 0xb3, 0x26, 0x1e ) );
			++lowDisk;
		}
		m_table->setItem( row, InvColumnDisk, diskItem );

		auto programsItem = new QTableWidgetItem;
		if( d.contains( QStringLiteral("software") ) )
		{
			programsItem->setData( Qt::DisplayRole, int( d[QStringLiteral("software")].toArray().size() ) );
		}
		m_table->setItem( row, InvColumnPrograms, programsItem );

		auto seenItem = new QTableWidgetItem( record.lastSeen.isValid() ?
			record.lastSeen.toLocalTime().toString( QStringLiteral("yyyy-MM-dd HH:mm") ) : QString{} );
		if( record.lastSeen.isValid() && state.inventoryOfflineDays > 0 && record.lastSeen.daysTo( now ) >= state.inventoryOfflineDays )
		{
			seenItem->setForeground( QColor( 0xb3, 0x26, 0x1e ) );
			++offline;
		}
		m_table->setItem( row, InvColumnLastSeen, seenItem );

		if( record.key == selectedKey )
		{
			m_table->selectRow( row );
		}
	}
	m_table->setSortingEnabled( true );
	m_table->resizeColumnsToContents();
	m_table->horizontalHeader()->setSectionResizeMode( InvColumnCpu, QHeaderView::Stretch );

	if( m_records.isEmpty() )
	{
		m_summary->setText( state.enabled ? tr( "No inventory yet - the gateway collects it within a few minutes." )
										  : tr( "The inventory is collected by the Aruni Gateway - enable it on the \"Gateway\" tab." ) );
	}
	else
	{
		m_summary->setText( tr( "%1 computers, %2 with a nearly full disk, %3 not online for a while. Computers with "
								"an older AruniControl only show when they were last seen." )
								.arg( shown ).arg( lowDisk ).arg( offline ) );
	}

	showDetails();
}



void InventoryView::showDetails()
{
	const auto selection = m_table->selectionModel()->selectedRows();
	if( selection.isEmpty() )
	{
		m_details->setHtml( QStringLiteral("<p style='color:gray'>%1</p>").arg( tr( "Select a computer to see details and its programs." ).toHtmlEscaped() ) );
		return;
	}

	const auto key = m_table->item( selection.first().row(), InvColumnName )->data( Qt::UserRole ).toString();
	const auto it = std::find_if( m_records.cbegin(), m_records.cend(), [&key]( const InventoryStore::Record& r ) { return r.key == key; } );
	if( it == m_records.cend() )
	{
		return;
	}
	const auto& d = it->data;
	const auto esc = []( const QString& text ) { return text.toHtmlEscaped(); };

	QString html = QStringLiteral("<h3>%1</h3><table cellspacing='2'>").arg( esc( it->name ) );
	const auto row = [&html, &esc]( const QString& label, const QString& value ) {
		if( value.trimmed().isEmpty() == false )
		{
			html += QStringLiteral("<tr><td style='color:gray;padding-right:12px'>%1</td><td>%2</td></tr>").arg( esc( label ), esc( value ) );
		}
	};
	row( tr( "Address" ), it->host );
	row( tr( "Model" ), QStringLiteral("%1 %2").arg( d[QStringLiteral("manufacturer")].toString(), d[QStringLiteral("model")].toString() ) );
	row( tr( "Serial number" ), d[QStringLiteral("serial")].toString() );
	row( tr( "Operating system" ), d[QStringLiteral("os")].toString() );
	row( tr( "Processor" ), QStringLiteral("%1 (%2)").arg( d[QStringLiteral("cpu")].toString() ).arg( tr( "%n core(s)", nullptr, d[QStringLiteral("cores")].toInt() ) ) );
	if( d.contains( QStringLiteral("ramMB") ) )
	{
		row( tr( "RAM" ), gb( d[QStringLiteral("ramMB")].toDouble() / 1024 ) );
	}
	for( const auto& value : d[QStringLiteral("disks")].toArray() )
	{
		const auto disk = value.toObject();
		row( tr( "Disk %1" ).arg( disk[QStringLiteral("path")].toString() ),
			 tr( "%1 free of %2" ).arg( gb( disk[QStringLiteral("freeGB")].toDouble() ), gb( disk[QStringLiteral("totalGB")].toDouble() ) ) );
	}
	for( const auto& value : d[QStringLiteral("network")].toArray() )
	{
		const auto net = value.toObject();
		row( net[QStringLiteral("name")].toString(), QStringLiteral("%1 · %2").arg( net[QStringLiteral("ip")].toString(), net[QStringLiteral("mac")].toString() ) );
	}
	row( tr( "User" ), d[QStringLiteral("user")].toString() );
	if( d.contains( QStringLiteral("uptimeHours") ) )
	{
		row( tr( "Running since" ), tr( "%n hour(s)", nullptr, d[QStringLiteral("uptimeHours")].toInt() ) );
	}
	row( tr( "AruniControl" ), d[QStringLiteral("version")].toString() );
	row( tr( "Collected" ), it->collected.isValid() ? it->collected.toLocalTime().toString( QStringLiteral("yyyy-MM-dd HH:mm") ) : tr( "not yet" ) );
	html += QStringLiteral("</table>");

	const auto software = d[QStringLiteral("software")].toArray();
	if( software.isEmpty() == false )
	{
		html += QStringLiteral("<h4>%1</h4><table cellspacing='1'>").arg( esc( tr( "Programs (%1)" ).arg( software.size() ) ) );
		for( const auto& value : software )
		{
			const auto program = value.toObject();
			html += QStringLiteral("<tr><td style='padding-right:12px'>%1</td><td style='color:gray;padding-right:12px'>%2</td><td style='color:gray'>%3</td></tr>")
						.arg( esc( program[QStringLiteral("name")].toString() ), esc( program[QStringLiteral("version")].toString() ),
							  esc( program[QStringLiteral("publisher")].toString() ) );
		}
		html += QStringLiteral("</table>");
	}
	m_details->setHtml( html );
}



void InventoryView::exportCsv( bool software )
{
	const auto defaultName = software ? QStringLiteral("inventaris-software.csv") : QStringLiteral("inventaris-komputer.csv");
	const auto fileName = QFileDialog::getSaveFileName( this, tr( "Export inventory" ),
		QDir( QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) ).filePath( defaultName ),
		tr( "CSV files (*.csv)" ) );
	if( fileName.isEmpty() )
	{
		return;
	}
	const bool ok = software ? InventoryStore::exportSoftwareCsv( fileName ) : InventoryStore::exportComputersCsv( fileName );
	if( ok == false )
	{
		QMessageBox::critical( this, tr( "Inventory" ), tr( "Cannot write %1" ).arg( fileName ) );
	}
}



void InventoryView::collectNow()
{
	QDir().mkpath( InventoryStore::directory() );
	QFile file( QDir( InventoryStore::directory() ).filePath( QStringLiteral("collect-now") ) );
	if( file.open( QFile::WriteOnly ) == false )
	{
		QMessageBox::critical( this, tr( "Inventory" ), tr( "Cannot write %1" ).arg( file.fileName() ) );
		return;
	}
	QMessageBox::information( this, tr( "Inventory" ), tr( "The gateway collects the inventory of all computers within the next "
															 "minutes - this list updates by itself." ) );
}
