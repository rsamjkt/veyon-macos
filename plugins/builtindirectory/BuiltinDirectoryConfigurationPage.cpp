/*
 * BuiltinDirectoryConfigurationPage.cpp - implementation of BuiltinDirectoryConfigurationPage
 *
 * Copyright (c) 2017-2026 Tobias Junghans <tobydox@veyon.io>
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

#include <QJsonObject>

#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>

#include "BuiltinDirectoryConfiguration.h"
#include "ComputerListFile.h"
#include "BuiltinDirectoryConfigurationPage.h"
#include "Configuration/UiMapping.h"
#include "NetworkObjectModel.h"
#include "ObjectManager.h"

#include "ui_BuiltinDirectoryConfigurationPage.h"

BuiltinDirectoryConfigurationPage::BuiltinDirectoryConfigurationPage( BuiltinDirectoryConfiguration& configuration, QWidget* parent ) :
	ConfigurationPage( parent ),
	ui(new Ui::BuiltinDirectoryConfigurationPage),
	m_configuration( configuration )
{
	ui->setupUi(this);

	populateLocations();

	connect( ui->locationTableWidget, &QTableWidget::currentItemChanged,
			 this, &BuiltinDirectoryConfigurationPage::populateComputers );

	// the whole list at once instead of typing every computer - replaces the
	// hint that CSV import is only possible on the command line
	ui->label_3->hide();
	auto transferBox = new QGroupBox( tr( "Import and export" ) );
	auto transferLayout = new QVBoxLayout( transferBox );
	auto hint = new QLabel( tr( "Make a table in Excel with the columns Room, Name, IP address and MAC (optional), save it as "
								"CSV and import it - or copy the rows in Excel and click \"Paste from Excel\"." ) );
	hint->setWordWrap( true );
	transferLayout->addWidget( hint );
	auto buttons = new QHBoxLayout;
	const auto addButton = [this, buttons]( const QString& text, void (BuiltinDirectoryConfigurationPage::*slot)() ) {
		auto button = new QPushButton( text );
		connect( button, &QPushButton::clicked, this, slot );
		buttons->addWidget( button );
	};
	addButton( tr( "Import from file…" ), &BuiltinDirectoryConfigurationPage::importFile );
	addButton( tr( "Paste from Excel" ), &BuiltinDirectoryConfigurationPage::importClipboard );
	addButton( tr( "Export…" ), &BuiltinDirectoryConfigurationPage::exportFile );
	addButton( tr( "Template…" ), &BuiltinDirectoryConfigurationPage::saveTemplate );
	buttons->addStretch( 1 );
	transferLayout->addLayout( buttons );
	ui->verticalLayout->addWidget( transferBox );
}



BuiltinDirectoryConfigurationPage::~BuiltinDirectoryConfigurationPage()
{
	delete ui;
}



void BuiltinDirectoryConfigurationPage::resetWidgets()
{
	populateLocations();

	ui->locationTableWidget->setCurrentCell( 0, 0 );
}



void BuiltinDirectoryConfigurationPage::connectWidgetsToProperties()
{
}



void BuiltinDirectoryConfigurationPage::applyConfiguration()
{
}



void BuiltinDirectoryConfigurationPage::addLocation()
{
	ObjectManager<NetworkObject> objectManager( m_configuration.networkObjects() );
	objectManager.add(NetworkObject(NetworkObject::Type::Location,
									objectManager.generateUniqueName(tr("New location")),
									{}, {}, {}, QUuid::createUuid()));
	m_configuration.setNetworkObjects( objectManager.objects() );

	populateLocations();

	ui->locationTableWidget->setCurrentCell( ui->locationTableWidget->rowCount()-1, 0 );
}



void BuiltinDirectoryConfigurationPage::updateLocation()
{
	auto currentLocationIndex = ui->locationTableWidget->currentIndex();
	if( currentLocationIndex.isValid() == false )
	{
		return;
	}

	ObjectManager<NetworkObject> objectManager( m_configuration.networkObjects() );
	objectManager.update( currentLocationObject() );
	m_configuration.setNetworkObjects( objectManager.objects() );

	populateLocations();

	ui->locationTableWidget->setCurrentIndex( currentLocationIndex );
}



void BuiltinDirectoryConfigurationPage::removeLocation()
{
	auto currentRow = ui->locationTableWidget->currentRow();

	ObjectManager<NetworkObject> objectManager( m_configuration.networkObjects() );
	objectManager.remove( currentLocationObject().uid(), true );
	m_configuration.setNetworkObjects( objectManager.objects() );

	populateLocations();

	if( currentRow > 0 )
	{
		ui->locationTableWidget->setCurrentCell( currentRow-1, 0 );
	}
	else if ( ui->locationTableWidget->rowCount() > 0 )
	{
		ui->locationTableWidget->setCurrentCell( currentRow, 0 );
	}
}



void BuiltinDirectoryConfigurationPage::moveLocationUp()
{
	const int row = ui->locationTableWidget->currentRow();

	if( row <= 0 )
	{
		return;
	}

	ObjectManager<NetworkObject> objectManager( m_configuration.networkObjects() );
	objectManager.moveUp( currentLocationObject().uid() );
	m_configuration.setNetworkObjects( objectManager.objects() );

	populateLocations();
	ui->locationTableWidget->setCurrentCell( row - 1, 0 );
}



void BuiltinDirectoryConfigurationPage::moveLocationDown()
{
	const int row = ui->locationTableWidget->currentRow();

	if( row < 0 || row >= ui->locationTableWidget->rowCount() - 1 )
	{
		return;
	}

	ObjectManager<NetworkObject> objectManager( m_configuration.networkObjects() );
	objectManager.moveDown( currentLocationObject().uid() );
	m_configuration.setNetworkObjects( objectManager.objects() );

	populateLocations();
	ui->locationTableWidget->setCurrentCell( row + 1, 0 );
}



void BuiltinDirectoryConfigurationPage::addComputer()
{
	auto currentLocationUid = currentLocationObject().uid();
	if( currentLocationUid.isNull() )
	{
		return;
	}

	ObjectManager<NetworkObject> objectManager( m_configuration.networkObjects() );
	objectManager.add(NetworkObject(NetworkObject::Type::Host,
									objectManager.generateUniqueName(tr("New computer")),
									{}, {}, {},
									QUuid::createUuid(),
									currentLocationUid));
	m_configuration.setNetworkObjects( objectManager.objects() );

	populateComputers();

	ui->computerTableWidget->setCurrentCell( ui->computerTableWidget->rowCount()-1, 0 );
}



void BuiltinDirectoryConfigurationPage::updateComputer()
{
	auto currentComputerIndex = ui->computerTableWidget->currentIndex();
	if( currentComputerIndex.isValid() == false )
	{
		return;
	}

	ObjectManager<NetworkObject> objectManager( m_configuration.networkObjects() );
	objectManager.update( currentComputerObject() );
	m_configuration.setNetworkObjects( objectManager.objects() );

	populateComputers();

	ui->computerTableWidget->setCurrentIndex( currentComputerIndex );
}



void BuiltinDirectoryConfigurationPage::removeComputer()
{
	ObjectManager<NetworkObject> objectManager( m_configuration.networkObjects() );
	objectManager.remove( currentComputerObject().uid() );
	m_configuration.setNetworkObjects( objectManager.objects() );

	populateComputers();
}



void BuiltinDirectoryConfigurationPage::moveComputerUp()
{
	const int row = ui->computerTableWidget->currentRow();

	if( row <= 0 )
	{
		return;
	}

	ObjectManager<NetworkObject> objectManager( m_configuration.networkObjects() );
	objectManager.moveUp( currentComputerObject().uid() );
	m_configuration.setNetworkObjects( objectManager.objects() );

	populateComputers();
	ui->computerTableWidget->setCurrentCell( row - 1, 0 );
}



void BuiltinDirectoryConfigurationPage::moveComputerDown()
{
	const int row = ui->computerTableWidget->currentRow();

	if( row < 0 || row >= ui->computerTableWidget->rowCount() - 1 )
	{
		return;
	}

	ObjectManager<NetworkObject> objectManager( m_configuration.networkObjects() );
	objectManager.moveDown( currentComputerObject().uid() );
	m_configuration.setNetworkObjects( objectManager.objects() );

	populateComputers();
	ui->computerTableWidget->setCurrentCell( row + 1, 0 );
}



void BuiltinDirectoryConfigurationPage::populateLocations()
{
	ui->locationTableWidget->setUpdatesEnabled( false );
	ui->locationTableWidget->clear();
	ui->locationTableWidget->setRowCount( 0 );

	ui->addComputerButton->setEnabled( false );

	int rowCount = 0;

	const auto networkObjects = m_configuration.networkObjects();
	for( const auto& networkObjectValue : networkObjects )
	{
		const NetworkObject networkObject( networkObjectValue.toObject() );
		if( networkObject.type() == NetworkObject::Type::Location )
		{
			auto item = new QTableWidgetItem( networkObject.name() );
			item->setData( NetworkObjectModel::UidRole, networkObject.uid() );
			ui->locationTableWidget->setRowCount( ++rowCount );
			ui->locationTableWidget->setItem( rowCount-1, 0, item );
		}
	}

	ui->locationTableWidget->setUpdatesEnabled( true );

	if( rowCount > 0 )
		ui->addComputerButton->setEnabled( true );
}



void BuiltinDirectoryConfigurationPage::populateComputers()
{
	auto parentUid = currentLocationObject().uid();

	ui->computerTableWidget->setUpdatesEnabled( false );
	ui->computerTableWidget->setRowCount( 0 );

	int rowCount = 0;

	const auto networkObjects = m_configuration.networkObjects();
	for( const auto& networkObjectValue : networkObjects )
	{
		const NetworkObject networkObject( networkObjectValue.toObject() );

		if( networkObject.type() == NetworkObject::Type::Host &&
			networkObject.parentUid() == parentUid )
		{
			auto nameItem = new QTableWidgetItem( networkObject.name() );
			nameItem->setData( NetworkObjectModel::UidRole, networkObject.uid() );
			nameItem->setData( NetworkObjectModel::ParentUidRole, networkObject.parentUid() );

			ui->computerTableWidget->setRowCount( rowCount+1 );
			ui->computerTableWidget->setItem( rowCount, 0, nameItem );
			ui->computerTableWidget->setItem( rowCount, 1, new QTableWidgetItem( networkObject.hostAddress() ) );
			ui->computerTableWidget->setItem( rowCount, 2, new QTableWidgetItem( networkObject.macAddress() ) );
			++rowCount;
		}
	}

	ui->computerTableWidget->setUpdatesEnabled( true );

	ui->addComputerButton->setEnabled( currentLocationObject().uid().isNull() == false );
}



NetworkObject BuiltinDirectoryConfigurationPage::currentLocationObject() const
{
	const auto selectedLocation = ui->locationTableWidget->currentItem();
	if( selectedLocation )
	{
		return NetworkObject( NetworkObject::Type::Location,
							  selectedLocation->text(),
							  {},
							  {},
							  {},
							  selectedLocation->data( NetworkObjectModel::UidRole ).toUuid(),
							  selectedLocation->data( NetworkObjectModel::ParentUidRole ).toUuid() );
	}

	return NetworkObject();
}



NetworkObject BuiltinDirectoryConfigurationPage::currentComputerObject() const
{
	const int row = ui->computerTableWidget->currentRow();
	if( row >= 0 )
	{
		auto nameItem = ui->computerTableWidget->item( row, 0 );
		auto hostAddressItem = ui->computerTableWidget->item( row, 1 );
		auto macAddressItem = ui->computerTableWidget->item( row, 2 );

		return NetworkObject( NetworkObject::Type::Host,
							  nameItem->text(),
							  hostAddressItem->text().trimmed(),
							  macAddressItem->text().trimmed(),
							  {},
							  nameItem->data( NetworkObjectModel::UidRole ).toUuid(),
							  nameItem->data( NetworkObjectModel::ParentUidRole ).toUuid() );
	}

	return NetworkObject();
}



void BuiltinDirectoryConfigurationPage::importFile()
{
	const auto fileName = QFileDialog::getOpenFileName( this, tr( "Import computers" ),
		QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ),
		tr( "Tables (*.csv *.txt *.tsv);;All files (*)" ) );
	if( fileName.isEmpty() )
	{
		return;
	}

	QFile file( fileName );
	if( file.open( QFile::ReadOnly ) == false )
	{
		QMessageBox::critical( this, tr( "Import computers" ), tr( "Could not open %1." ).arg( fileName ) );
		return;
	}

	// Excel saves "CSV (Comma delimited)" in the Windows code page, "CSV UTF-8"
	// in UTF-8 - fall back when the text is not valid UTF-8
	const auto data = file.readAll();
	auto text = QString::fromUtf8( data );
	if( text.contains( QChar::ReplacementCharacter ) )
	{
		text = QString::fromLatin1( data );
	}
	importText( text );
}



void BuiltinDirectoryConfigurationPage::importClipboard()
{
	const auto text = QApplication::clipboard()->text();
	if( text.trimmed().isEmpty() )
	{
		QMessageBox::information( this, tr( "Paste from Excel" ),
								  tr( "The clipboard is empty. Select the rows in Excel (with or without the heading row), "
									  "press Ctrl+C and click this button again." ) );
		return;
	}
	importText( text );
}



void BuiltinDirectoryConfigurationPage::importText( const QString& text )
{
	const auto parsed = ComputerListFile::parse( text, currentLocationObject().name() );
	const QString problems = parsed.problems.mid( 0, 12 ).join( QLatin1Char('\n') ) +
						  ( parsed.problems.size() > 12 ? QStringLiteral("\n…") : QString{} );

	if( parsed.rows.isEmpty() )
	{
		QMessageBox::warning( this, tr( "Import computers" ),
							  tr( "No computers found. Expected columns: Room, Name, IP address, MAC." ) +
							  ( problems.isEmpty() ? QString{} : QStringLiteral("\n\n") + problems ) );
		return;
	}

	QMessageBox question( QMessageBox::Question, tr( "Import computers" ),
						  tr( "%1 computers found. Add them to the current list or replace the whole list?" )
							  .arg( parsed.rows.size() ), QMessageBox::Cancel, this );
	auto addButton = question.addButton( tr( "Add" ), QMessageBox::AcceptRole );
	auto replaceButton = question.addButton( tr( "Replace all" ), QMessageBox::DestructiveRole );
	question.setDefaultButton( addButton );
	if( problems.isEmpty() == false )
	{
		question.setDetailedText( problems );
	}
	question.exec();
	if( question.clickedButton() != addButton && question.clickedButton() != replaceButton )
	{
		return;
	}

	auto objects = m_configuration.networkObjects();
	const auto result = ComputerListFile::merge( objects, parsed.rows, question.clickedButton() == replaceButton );
	m_configuration.setNetworkObjects( objects );
	populateLocations();

	QMessageBox::information( this, tr( "Import computers" ),
		tr( "%1 rooms and %2 computers added, %3 computers updated. Click Apply to save." )
			.arg( result.locationsAdded ).arg( result.computersAdded ).arg( result.computersUpdated ) +
		( problems.isEmpty() ? QString{} : QStringLiteral("\n\n") + tr( "Skipped:" ) + QLatin1Char('\n') + problems ) );
}



void BuiltinDirectoryConfigurationPage::exportFile()
{
	const auto fileName = QFileDialog::getSaveFileName( this, tr( "Export computers" ),
		QDir( QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) ).filePath( tr( "computers.csv" ) ),
		tr( "CSV files (*.csv)" ) );
	if( fileName.isEmpty() )
	{
		return;
	}

	QFile file( fileName );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) == false ||
		file.write( ComputerListFile::toCsv( m_configuration.networkObjects() ) ) < 0 )
	{
		QMessageBox::critical( this, tr( "Export computers" ), tr( "Could not write %1." ).arg( fileName ) );
	}
}



void BuiltinDirectoryConfigurationPage::saveTemplate()
{
	const auto fileName = QFileDialog::getSaveFileName( this, tr( "Save template" ),
		QDir( QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) ).filePath( tr( "computer-list-template.csv" ) ),
		tr( "CSV files (*.csv)" ) );
	if( fileName.isEmpty() )
	{
		return;
	}

	QFile file( fileName );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) == false || file.write( ComputerListFile::templateCsv() ) < 0 )
	{
		QMessageBox::critical( this, tr( "Save template" ), tr( "Could not write %1." ).arg( fileName ) );
	}
}
