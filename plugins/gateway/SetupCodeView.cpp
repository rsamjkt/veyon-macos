/*
 * SetupCodeView.cpp - installation code in the Configurator
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

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QFile>
#include <QComboBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "GatewayState.h"
#include "SetupCode.h"
#include "SetupCodeView.h"
#include "VeyonServiceControl.h"


SetupCodeView::SetupCodeView( QWidget* parent ) :
	QWidget( parent )
{
	auto layout = new QVBoxLayout( this );

	auto createBox = new QGroupBox( tr( "Create an installation code (on the admin computer)" ) );
	auto createLayout = new QVBoxLayout( createBox );
	auto intro = new QLabel( tr( "One code sets up any number of computers: no key import on each computer." ) );
	intro->setWordWrap( true );
	createLayout->addWidget( intro );

	createLayout->addWidget( new QLabel( tr( "Authentication keys:" ) ) );
	m_keys = new QListWidget;
	m_keys->setMaximumHeight( 64 );
	createLayout->addWidget( m_keys );

	m_privateKeys = new QCheckBox( tr( "Include private keys (teacher/Master computers only)" ) );
	m_privateKeys->setToolTip( tr( "Whoever has a code with private keys can control the computers - keep it secret." ) );
	createLayout->addWidget( m_privateKeys );
	m_computers = new QCheckBox( tr( "Include the list of rooms and computers" ) );
	createLayout->addWidget( m_computers );
	m_roaming = new QCheckBox( tr( "Roaming laptops: stay reachable outside the office" ) );
	createLayout->addWidget( m_roaming );

	auto createButton = new QPushButton( tr( "Create code" ) );
	auto createRow = new QHBoxLayout;
	createRow->addWidget( createButton );
	createRow->addStretch( 1 );
	createLayout->addLayout( createRow );

	m_code = new QPlainTextEdit;
	m_code->setReadOnly( true );
	m_code->setMaximumHeight( 90 );
	createLayout->addWidget( m_code );

	auto codeButtons = new QHBoxLayout;
	m_copyButton = new QPushButton( tr( "Copy" ) );
	m_saveButton = new QPushButton( tr( "Save as aruni-setup.txt..." ) );
	codeButtons->addWidget( m_copyButton );
	codeButtons->addWidget( m_saveButton );
	codeButtons->addStretch( 1 );
	createLayout->addLayout( codeButtons );

	auto commandRow = new QHBoxLayout;
	commandRow->addWidget( new QLabel( tr( "One-line installation (agent):" ) ) );
	m_commandKind = new QComboBox;
	m_commandKind->addItem( tr( "Windows - PowerShell (Administrator)" ), QStringLiteral("powershell") );
	m_commandKind->addItem( tr( "Windows - Command Prompt (Administrator)" ), QStringLiteral("cmd") );
	m_commandKind->addItem( tr( "Mac - Terminal" ), QStringLiteral("mac") );
	commandRow->addWidget( m_commandKind, 1 );
	m_copyCommandButton = new QPushButton( tr( "Copy command" ) );
	commandRow->addWidget( m_copyCommandButton );
	createLayout->addLayout( commandRow );
	m_command = new QPlainTextEdit;
	m_command->setReadOnly( true );
	m_command->setMaximumHeight( 70 );
	m_command->setFont( QFontDatabase::systemFont( QFontDatabase::FixedFont ) );
	createLayout->addWidget( m_command );

	m_codeHint = new QLabel;
	m_codeHint->setWordWrap( true );
	m_codeHint->setTextInteractionFlags( Qt::TextSelectableByMouse );
	m_codeHint->setTextFormat( Qt::PlainText );
	createLayout->addWidget( m_codeHint );
	layout->addWidget( createBox );

	auto applyBox = new QGroupBox( tr( "Set up this computer with a code" ) );
	auto applyLayout = new QVBoxLayout( applyBox );
	m_input = new QPlainTextEdit;
	m_input->setPlaceholderText( QStringLiteral("ARUNISETUP1:...") );
	m_input->setMaximumHeight( 70 );
	applyLayout->addWidget( m_input );
	auto applyButton = new QPushButton( tr( "Apply code" ) );
	auto applyRow = new QHBoxLayout;
	applyRow->addWidget( applyButton );
	applyRow->addStretch( 1 );
	applyLayout->addLayout( applyRow );
	layout->addWidget( applyBox );
	layout->addStretch( 1 );

	connect( createButton, &QPushButton::clicked, this, &SetupCodeView::createCode );
	connect( m_copyButton, &QPushButton::clicked, this, [this]() {
		QApplication::clipboard()->setText( m_code->toPlainText() );
	} );
	connect( m_saveButton, &QPushButton::clicked, this, &SetupCodeView::saveFile );
	connect( m_commandKind, &QComboBox::currentIndexChanged, this, &SetupCodeView::updateCommand );
	connect( m_copyCommandButton, &QPushButton::clicked, this, [this]() {
		QApplication::clipboard()->setText( m_command->toPlainText() );
	} );
	connect( applyButton, &QPushButton::clicked, this, &SetupCodeView::applyCode );
	for( auto checkBox : { m_privateKeys, m_computers, m_roaming } )
	{
		connect( checkBox, &QCheckBox::toggled, m_code, &QPlainTextEdit::clear );
	}
	connect( m_code, &QPlainTextEdit::textChanged, this, [this]() {
		const bool haveCode = m_code->toPlainText().isEmpty() == false;
		m_copyButton->setEnabled( haveCode );
		m_saveButton->setEnabled( haveCode );
		m_copyCommandButton->setEnabled( haveCode );
		updateCommand();
		if( haveCode == false )
		{
			m_codeHint->clear();
		}
	} );

	refresh();
	m_copyButton->setEnabled( false );
	m_saveButton->setEnabled( false );
	m_copyCommandButton->setEnabled( false );
}



QString SetupCodeView::installCommand( const QString& kind, const QString& code )
{
	const auto site = QStringLiteral("https://arunicontrol.arunihealth.id");
	if( kind == QStringLiteral("cmd") )
	{
		return QStringLiteral("powershell -NoProfile -ExecutionPolicy Bypass -Command \"$env:ARUNI_KEY='%1'; irm %2/pasang.ps1 | iex\"").arg( code, site );
	}
	if( kind == QStringLiteral("mac") )
	{
		return QStringLiteral("curl -fsSL %2/pasang.sh | ARUNI_KEY='%1' bash").arg( code, site );
	}
	return QStringLiteral("$env:ARUNI_KEY='%1'; irm %2/pasang.ps1 | iex").arg( code, site );
}



void SetupCodeView::updateCommand()
{
	const auto code = m_code->toPlainText().trimmed();
	// PowerShell keeps a plain text history of commands - a secret code must
	// not end up there
	const bool secret = code.isEmpty() == false && m_privateKeys->isChecked();
	m_copyCommandButton->setEnabled( code.isEmpty() == false && secret == false );
	m_command->setPlainText( code.isEmpty() ? QString{}
							 : secret ? tr( "Not offered for codes with private keys (the command would stay in the "
											"command history). Use aruni-setup.txt instead." )
									  : installCommand( m_commandKind->currentData().toString(), code ) );
}



void SetupCodeView::refresh()
{
	QStringList checked;
	for( int i = 0; i < m_keys->count(); ++i )
	{
		if( m_keys->item( i )->checkState() == Qt::Checked )
		{
			checked.append( m_keys->item( i )->text() );
		}
	}
	const bool first = m_keys->count() == 0;

	m_keys->clear();
	const auto keys = SetupCode::availableKeys();
	for( const auto& name : keys )
	{
		auto item = new QListWidgetItem( name, m_keys );
		item->setFlags( Qt::ItemIsEnabled | Qt::ItemIsUserCheckable );
		item->setCheckState( first || checked.contains( name ) ? Qt::Checked : Qt::Unchecked );
	}
	if( keys.isEmpty() )
	{
		auto item = new QListWidgetItem( tr( "No keys yet - create them on the \"Authentication keys\" page." ), m_keys );
		item->setFlags( Qt::NoItemFlags );
	}

	const bool gateway = GatewayState::load().enabled;
	m_roaming->setEnabled( gateway );
	m_roaming->setToolTip( gateway ? QString{} : tr( "Enable the Aruni Gateway on the \"Gateway\" tab first." ) );
	if( gateway == false )
	{
		m_roaming->setChecked( false );
	}
}



void SetupCodeView::createCode()
{
	SetupCode::Options options;
	for( int i = 0; i < m_keys->count(); ++i )
	{
		if( m_keys->item( i )->flags() & Qt::ItemIsUserCheckable && m_keys->item( i )->checkState() == Qt::Checked )
		{
			options.keyNames.append( m_keys->item( i )->text() );
		}
	}
	options.includePrivateKeys = m_privateKeys->isChecked();
	options.includeComputers = m_computers->isChecked();
	options.includeRoaming = m_roaming->isChecked();

	if( options.keyNames.isEmpty() && options.includeComputers == false && options.includeRoaming == false )
	{
		QMessageBox::warning( this, tr( "Installation code" ), tr( "Choose at least one key or option." ) );
		return;
	}

	QString error;
	const auto code = SetupCode::create( options, &error );
	if( code.isEmpty() )
	{
		QMessageBox::critical( this, tr( "Installation code" ), error );
		return;
	}

	m_code->setPlainText( code );

	QString hint = tr( "Easiest: run the one-line command above on each computer, like installing an agent. It downloads "
					   "the latest version, checks it and installs it without windows and without shortcuts. The code "
					   "stays on the computers - it is not sent anywhere." );
	hint += QLatin1Char('\n') + tr( "Without internet: save the code as aruni-setup.txt next to AruniControl-Setup.exe and run "
									"\"AruniControl-Setup.exe /S /AGENT\"." );
	if( options.includePrivateKeys )
	{
		hint.prepend( tr( "Keep this code secret: whoever has it can control the computers." ) + QLatin1Char('\n') );
	}
	m_codeHint->setText( hint );
}



void SetupCodeView::saveFile()
{
	const auto fileName = QFileDialog::getSaveFileName( this, tr( "Save installation code" ), QStringLiteral("aruni-setup.txt"),
														tr( "Text files (*.txt)" ) );
	if( fileName.isEmpty() )
	{
		return;
	}

	QFile file( fileName );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) == false || file.write( m_code->toPlainText().toUtf8() ) < 0 )
	{
		QMessageBox::critical( this, tr( "Installation code" ), tr( "Cannot write %1" ).arg( fileName ) );
	}
}



void SetupCodeView::applyCode()
{
	const auto content = SetupCode::decode( m_input->toPlainText() );
	if( content.isValid() == false )
	{
		QMessageBox::warning( this, tr( "Installation code" ), tr( "This is not a valid installation code." ) );
		return;
	}

	if( QMessageBox::question( this, tr( "Installation code" ),
							   tr( "Set up this computer with:\n\n%1\n\nContinue?" ).arg( SetupCode::describe( content ).join( QLatin1Char('\n') ) ) )
		!= QMessageBox::Yes )
	{
		return;
	}

	QStringList report;
	QString error;
	if( SetupCode::apply( content, report, error ) == false )
	{
		QMessageBox::critical( this, tr( "Installation code" ), error );
		return;
	}

	// the server uses the new keys and settings after a restart
	VeyonServiceControl( this ).restartService();

	m_input->clear();
	refresh();
	QMessageBox::information( this, tr( "Installation code" ),
							  report.join( QLatin1Char('\n') ) + QStringLiteral("\n\n") +
							  tr( "Close and reopen the Configurator to see the changes." ) );
}
