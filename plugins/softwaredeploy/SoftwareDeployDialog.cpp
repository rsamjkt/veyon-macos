/*
 * SoftwareDeployDialog.cpp - install and remove software on many computers
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

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include "SoftwareDeployDialog.h"
#include "SoftwareDeployFeaturePlugin.h"


namespace {

const QColor OkColor( 0x1e, 0x8e, 0x3e );
const QColor ErrorColor( 0xb3, 0x26, 0x1e );

QSettings deploySettings()
{
	return QSettings( QStringLiteral("AruniControl"), QStringLiteral("SoftwareDeploy") );
}

}



SoftwareDeployDialog::SoftwareDeployDialog( SoftwareDeployFeaturePlugin* plugin, const ComputerControlInterfaceList& computers,
											QWidget* parent ) :
	QDialog( parent ),
	m_plugin( plugin ),
	m_computers( computers )
{
	setWindowTitle( tr( "Install software" ) );
	resize( 640, 640 );

	auto layout = new QVBoxLayout( this );
	layout->addWidget( new QLabel( tr( "%n selected computer(s)", nullptr, int( computers.size() ) ) ) );

	m_tabs = new QTabWidget;
	layout->addWidget( m_tabs );

	// ---- install
	auto installPage = new QWidget;
	auto installLayout = new QFormLayout( installPage );
	auto fileRow = new QHBoxLayout;
	m_file = new QLineEdit;
	m_file->setPlaceholderText( tr( "Installer (.msi or .exe for Windows, .pkg for Mac)" ) );
	m_file->setReadOnly( true );
	auto browseButton = new QPushButton( tr( "Choose..." ) );
	fileRow->addWidget( m_file, 1 );
	fileRow->addWidget( browseButton );
	installLayout->addRow( tr( "Installer" ), fileRow );

	m_preset = new QComboBox;
	m_preset->addItem( tr( "Automatic (.msi: /qn, .exe: /S)" ), QStringLiteral("auto") );
	m_preset->addItem( tr( "NSIS installer" ), QStringLiteral("/S") );
	m_preset->addItem( tr( "Inno Setup installer" ), QStringLiteral("/VERYSILENT /SUPPRESSMSGBOXES /NORESTART") );
	m_preset->addItem( tr( "Windows Installer (.msi)" ), QStringLiteral("/qn /norestart") );
	m_preset->addItem( tr( "InstallShield" ), QStringLiteral("/s /v\"/qn\"") );
	m_preset->addItem( tr( "Own parameters" ), QStringLiteral("custom") );
	installLayout->addRow( tr( "Silent installation" ), m_preset );
	m_arguments = new QLineEdit;
	installLayout->addRow( tr( "Parameters" ), m_arguments );
	auto installHint = new QLabel( tr( "The parameters make the installer run without questions - look them up on the "
									   "website of the program if unsure. Test with one computer first." ) );
	installHint->setWordWrap( true );
	installHint->setStyleSheet( QStringLiteral("color: gray;") );
	installLayout->addRow( installHint );
	m_installButton = new QPushButton( tr( "Install" ) );
	m_installButton->setEnabled( false );
	installLayout->addRow( m_installButton );
	m_tabs->addTab( installPage, tr( "Install" ) );

	// ---- uninstall
	auto uninstallPage = new QWidget;
	auto uninstallLayout = new QVBoxLayout( uninstallPage );
	m_programFilter = new QLineEdit;
	m_programFilter->setPlaceholderText( tr( "Search program..." ) );
	m_programFilter->setClearButtonEnabled( true );
	uninstallLayout->addWidget( m_programFilter );
	m_programs = new QTableWidget( 0, 3 );
	m_programs->setHorizontalHeaderLabels( { tr( "Program" ), tr( "Version" ), tr( "Computers" ) } );
	m_programs->setSelectionBehavior( QAbstractItemView::SelectRows );
	m_programs->setSelectionMode( QAbstractItemView::SingleSelection );
	m_programs->setEditTriggers( QAbstractItemView::NoEditTriggers );
	m_programs->verticalHeader()->hide();
	m_programs->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::Stretch );
	uninstallLayout->addWidget( m_programs, 1 );
	auto uninstallRow = new QHBoxLayout;
	m_uninstallArguments = new QLineEdit;
	m_uninstallArguments->setPlaceholderText( tr( "Extra parameters (optional)" ) );
	uninstallRow->addWidget( m_uninstallArguments, 1 );
	m_uninstallButton = new QPushButton( tr( "Remove" ) );
	m_uninstallButton->setEnabled( false );
	uninstallRow->addWidget( m_uninstallButton );
	uninstallLayout->addLayout( uninstallRow );
	m_tabs->addTab( uninstallPage, tr( "Remove" ) );

	// ---- states
	m_states = new QTableWidget( int( computers.size() ), 2 );
	m_states->setHorizontalHeaderLabels( { tr( "Computer" ), tr( "State" ) } );
	m_states->verticalHeader()->hide();
	m_states->setEditTriggers( QAbstractItemView::NoEditTriggers );
	m_states->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::Stretch );
	for( int i = 0; i < computers.size(); ++i )
	{
		m_states->setItem( i, 0, new QTableWidgetItem( computers.at( i )->computer().displayName() ) );
		m_states->setItem( i, 1, new QTableWidgetItem );
	}
	m_states->resizeColumnToContents( 0 );
	layout->addWidget( m_states, 1 );

	m_summary = new QLabel;
	layout->addWidget( m_summary );

	auto buttons = new QDialogButtonBox( QDialogButtonBox::Close );
	connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
	layout->addWidget( buttons );

	connect( browseButton, &QPushButton::clicked, this, &SoftwareDeployDialog::chooseFile );
	connect( m_preset, &QComboBox::currentIndexChanged, this, &SoftwareDeployDialog::updatePreset );
	connect( m_installButton, &QPushButton::clicked, this, &SoftwareDeployDialog::startInstall );
	connect( m_uninstallButton, &QPushButton::clicked, this, &SoftwareDeployDialog::startUninstall );
	connect( m_programs, &QTableWidget::itemSelectionChanged, this, [this]() {
		m_uninstallButton->setEnabled( m_programs->selectionModel()->hasSelection() );
	} );
	connect( m_programFilter, &QLineEdit::textChanged, this, [this]( const QString& text ) {
		for( int row = 0; row < m_programs->rowCount(); ++row )
		{
			m_programs->setRowHidden( row, m_programs->item( row, 0 )->text().contains( text, Qt::CaseInsensitive ) == false );
		}
	} );
	connect( m_plugin, &SoftwareDeployFeaturePlugin::statusReceived, this, &SoftwareDeployDialog::onStatus );
	connect( m_plugin, &SoftwareDeployFeaturePlugin::softwareReceived, this, &SoftwareDeployDialog::onSoftware );

	// the list of programs for removing
	m_plugin->querySoftware( computers );
	updatePreset();
}



void SoftwareDeployDialog::chooseFile()
{
	auto settings = deploySettings();
	const auto fileName = QFileDialog::getOpenFileName( this, tr( "Choose installer" ),
		settings.value( QStringLiteral("folder") ).toString(), tr( "Installers (*.msi *.exe *.pkg)" ) );
	if( fileName.isEmpty() )
	{
		return;
	}
	settings.setValue( QStringLiteral("folder"), QFileInfo( fileName ).absolutePath() );

	m_file->setText( fileName );
	m_fileSize = QFileInfo( fileName ).size();
	m_installButton->setText( tr( "Install %1 (%2 MB) on %n computer(s)", nullptr, int( m_computers.size() ) )
								  .arg( QFileInfo( fileName ).fileName() )
								  .arg( QLocale().toString( m_fileSize / 1048576.0, 'f', 1 ) ) );
	m_installButton->setEnabled( true );
	updatePreset();
}



void SoftwareDeployDialog::updatePreset()
{
	const auto preset = m_preset->currentData().toString();
	m_arguments->setReadOnly( preset != QStringLiteral("custom") );
	if( preset == QStringLiteral("auto") )
	{
		m_arguments->setText( SoftwareDeployFeaturePlugin::defaultArguments( m_file->text() ) );
	}
	else if( preset != QStringLiteral("custom") )
	{
		m_arguments->setText( preset );
	}
}



void SoftwareDeployDialog::startInstall()
{
	if( isBusy() && QMessageBox::question( this, windowTitle(), tr( "An installation is still running. Start anyway?" ) ) != QMessageBox::Yes )
	{
		return;
	}
	if( QMessageBox::question( this, windowTitle(),
							   tr( "Install %1 on %n computer(s) now? Users are not asked.", nullptr, int( m_computers.size() ) )
								   .arg( QFileInfo( m_file->text() ).fileName() ) ) != QMessageBox::Yes )
	{
		return;
	}

	resetStates( tr( "Sending..." ) );
	m_plugin->install( m_file->text(), m_arguments->text().trimmed(), m_computers );
}



void SoftwareDeployDialog::startUninstall()
{
	const auto rows = m_programs->selectionModel()->selectedRows();
	if( rows.isEmpty() )
	{
		return;
	}
	const auto id = m_programs->item( rows.first().row(), 0 )->data( Qt::UserRole ).toString();
	const auto name = m_programs->item( rows.first().row(), 0 )->text();
	if( QMessageBox::question( this, windowTitle(),
							   tr( "Remove %1 from all selected computers that have it? Users are not asked." ).arg( name ) )
		!= QMessageBox::Yes )
	{
		return;
	}

	resetStates( tr( "Removing..." ) );
	m_plugin->uninstall( id, m_uninstallArguments->text().trimmed(), m_computers );
}



void SoftwareDeployDialog::onStatus( ComputerControlInterface::Pointer computer, const QString& state, qint64 received,
									 int exitCode, const QString& error )
{
	Q_UNUSED(exitCode)

	m_stateOf[computer.data()] = state;
	if( state == QStringLiteral("receiving") )
	{
		const auto percent = m_fileSize > 0 ? int( received * 100 / m_fileSize ) : 0;
		setState( computer, tr( "Receiving %1%" ).arg( percent ) );
	}
	else if( state == QStringLiteral("installing") )
	{
		setState( computer, tr( "Running..." ) );
	}
	else if( state == QStringLiteral("done") )
	{
		setState( computer, error.isEmpty() ? tr( "Done" ) : tr( "Done - %1" ).arg( error ), OkColor );
	}
	else
	{
		setState( computer, tr( "Failed: %1" ).arg( error ), ErrorColor );
	}

	int done = 0;
	int failed = 0;
	for( const auto& value : std::as_const( m_stateOf ) )
	{
		done += value == QStringLiteral("done") ? 1 : 0;
		failed += value == QStringLiteral("failed") ? 1 : 0;
	}
	m_summary->setText( tr( "%1 done, %2 failed, %3 still running" ).arg( done ).arg( failed )
							.arg( m_stateOf.size() - done - failed ) );
}



void SoftwareDeployDialog::onSoftware( ComputerControlInterface::Pointer computer, const QJsonArray& software )
{
	Q_UNUSED(computer)

	for( const auto& value : software )
	{
		const auto object = value.toObject();
		if( object[QStringLiteral("removable")].toBool() == false &&
			object[QStringLiteral("id")].toString().endsWith( QStringLiteral(".app") ) == false )
		{
			continue;
		}
		const auto id = object[QStringLiteral("id")].toString();
		auto it = std::find_if( m_programList.begin(), m_programList.end(), [&id]( const Program& p ) { return p.id == id; } );
		if( it == m_programList.end() )
		{
			m_programList.append( { id, object[QStringLiteral("name")].toString(), object[QStringLiteral("version")].toString(), 1 } );
		}
		else
		{
			++it->count;
		}
	}

	std::sort( m_programList.begin(), m_programList.end(), []( const Program& a, const Program& b ) {
		return a.name.compare( b.name, Qt::CaseInsensitive ) < 0;
	} );

	m_programs->setRowCount( int( m_programList.size() ) );
	for( int row = 0; row < m_programList.size(); ++row )
	{
		const auto& program = m_programList.at( row );
		auto nameItem = new QTableWidgetItem( program.name );
		nameItem->setData( Qt::UserRole, program.id );
		m_programs->setItem( row, 0, nameItem );
		m_programs->setItem( row, 1, new QTableWidgetItem( program.version ) );
		m_programs->setItem( row, 2, new QTableWidgetItem( QString::number( program.count ) ) );
		m_programs->setRowHidden( row, program.name.contains( m_programFilter->text(), Qt::CaseInsensitive ) == false );
	}
}



void SoftwareDeployDialog::setState( const ComputerControlInterface::Pointer& computer, const QString& text, const QColor& color )
{
	const auto index = m_computers.indexOf( computer );
	if( index < 0 )
	{
		return;
	}
	auto item = m_states->item( int( index ), 1 );
	item->setText( text );
	item->setToolTip( text );
	item->setForeground( color.isValid() ? QBrush( color ) : QBrush() );
}



void SoftwareDeployDialog::resetStates( const QString& text )
{
	m_stateOf.clear();
	for( const auto& computer : std::as_const( m_computers ) )
	{
		setState( computer, computer->state() == ComputerControlInterface::State::Connected ? text : tr( "Not connected" ),
				  computer->state() == ComputerControlInterface::State::Connected ? QColor{} : ErrorColor );
		if( computer->state() == ComputerControlInterface::State::Connected )
		{
			m_stateOf[computer.data()] = QStringLiteral("pending");
		}
	}
	m_summary->clear();
}



bool SoftwareDeployDialog::isBusy() const
{
	return std::any_of( m_stateOf.cbegin(), m_stateOf.cend(), []( const QString& state ) {
		return state != QStringLiteral("done") && state != QStringLiteral("failed");
	} );
}
