/*
 * RemoteCommandDialog.cpp - run commands on many computers
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
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

#include "RemoteCommandDialog.h"
#include "RemoteCommandFeaturePlugin.h"


namespace {

struct Example
{
	const char* name;
	const char* shell;
	const char* script;
};

// safe, read-only or easily undone examples
const Example examples[] = {
	{ QT_TRANSLATE_NOOP( "RemoteCommandDialog", "Network settings" ), "powershell", "Get-NetIPConfiguration | Format-List" },
	{ QT_TRANSLATE_NOOP( "RemoteCommandDialog", "Free disk space" ), "powershell",
	  "Get-PSDrive -PSProvider FileSystem | Select-Object Name, @{n='Free GB';e={[math]::Round($_.Free/1GB,1)}}, @{n='Used GB';e={[math]::Round($_.Used/1GB,1)}}" },
	{ QT_TRANSLATE_NOOP( "RemoteCommandDialog", "Clean temporary files" ), "powershell",
	  "Remove-Item \"$env:windir\\Temp\\*\" -Recurse -Force -ErrorAction SilentlyContinue\nGet-ChildItem C:\\Users -Directory | ForEach-Object { Remove-Item \"$($_.FullName)\\AppData\\Local\\Temp\\*\" -Recurse -Force -ErrorAction SilentlyContinue }\n'Done'" },
	{ QT_TRANSLATE_NOOP( "RemoteCommandDialog", "Apply group policies" ), "cmd", "gpupdate /force" },
	{ QT_TRANSLATE_NOOP( "RemoteCommandDialog", "Restart printing" ), "powershell", "Restart-Service Spooler -Force\n'Printing restarted'" },
	{ QT_TRANSLATE_NOOP( "RemoteCommandDialog", "Installed Windows updates" ), "powershell",
	  "Get-HotFix | Sort-Object InstalledOn -Descending | Select-Object -First 10 HotFixID, InstalledOn" },
	{ QT_TRANSLATE_NOOP( "RemoteCommandDialog", "Mac: disk space" ), "sh", "df -h /" },
};

}



RemoteCommandDialog::RemoteCommandDialog( RemoteCommandFeaturePlugin* plugin, const ComputerControlInterfaceList& computers,
										  QWidget* parent ) :
	QDialog( parent ),
	m_plugin( plugin ),
	m_computers( computers )
{
	setWindowTitle( tr( "Run command" ) );
	resize( 760, 680 );

	auto layout = new QVBoxLayout( this );
	layout->addWidget( new QLabel( tr( "%n selected computer(s). Windows runs the command as the system account, "
									   "macOS as the logged-on user.", nullptr, int( computers.size() ) ) ) );

	auto row = new QHBoxLayout;
	m_examples = new QComboBox;
	m_examples->addItem( tr( "Examples..." ) );
	for( const auto& example : examples )
	{
		m_examples->addItem( tr( example.name ) );
	}
	row->addWidget( m_examples, 1 );
	row->addWidget( new QLabel( tr( "Shell" ) ) );
	m_shell = new QComboBox;
	m_shell->addItem( QStringLiteral("PowerShell (Windows)"), QStringLiteral("powershell") );
	m_shell->addItem( QStringLiteral("CMD (Windows)"), QStringLiteral("cmd") );
	m_shell->addItem( QStringLiteral("sh (macOS)"), QStringLiteral("sh") );
	row->addWidget( m_shell );
	row->addWidget( new QLabel( tr( "Time limit" ) ) );
	m_timeout = new QSpinBox;
	m_timeout->setRange( 5, 1800 );
	m_timeout->setSuffix( tr( " s" ) );
	row->addWidget( m_timeout );
	layout->addLayout( row );

	const QSettings settings( QStringLiteral("AruniControl"), QStringLiteral("RemoteCommand") );
	m_script = new QPlainTextEdit( settings.value( QStringLiteral("script") ).toString() );
	m_script->setFont( QFontDatabase::systemFont( QFontDatabase::FixedFont ) );
	m_script->setPlaceholderText( tr( "Command or script, e.g.\nipconfig /all" ) );
	m_shell->setCurrentIndex( qMax( 0, m_shell->findData( settings.value( QStringLiteral("shell"), RemoteCommandFeaturePlugin::defaultShell() ) ) ) );
	m_timeout->setValue( settings.value( QStringLiteral("timeout"), 60 ).toInt() );

	auto splitter = new QSplitter( Qt::Vertical );
	splitter->addWidget( m_script );

	m_results = new QTableWidget( int( computers.size() ), 2 );
	m_results->setHorizontalHeaderLabels( { tr( "Computer" ), tr( "Result" ) } );
	m_results->setSelectionBehavior( QAbstractItemView::SelectRows );
	m_results->setSelectionMode( QAbstractItemView::SingleSelection );
	m_results->setEditTriggers( QAbstractItemView::NoEditTriggers );
	m_results->verticalHeader()->hide();
	m_results->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::Stretch );
	for( int i = 0; i < computers.size(); ++i )
	{
		m_results->setItem( i, 0, new QTableWidgetItem( computers.at( i )->computer().displayName() ) );
		m_results->setItem( i, 1, new QTableWidgetItem );
	}
	m_results->resizeColumnToContents( 0 );
	splitter->addWidget( m_results );

	m_output = new QTextBrowser;
	m_output->setFont( QFontDatabase::systemFont( QFontDatabase::FixedFont ) );
	m_output->setPlaceholderText( tr( "Select a computer to see its output." ) );
	splitter->addWidget( m_output );
	layout->addWidget( splitter, 1 );

	m_summary = new QLabel;
	layout->addWidget( m_summary );

	auto buttons = new QDialogButtonBox( QDialogButtonBox::Close );
	auto runButton = buttons->addButton( tr( "Run" ), QDialogButtonBox::ActionRole );
	connect( runButton, &QPushButton::clicked, this, &RemoteCommandDialog::runCommand );
	connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
	layout->addWidget( buttons );

	connect( m_examples, &QComboBox::activated, this, [this]( int index ) {
		if( index > 0 )
		{
			const auto& example = examples[index - 1];
			m_script->setPlainText( QString::fromUtf8( example.script ) );
			m_shell->setCurrentIndex( m_shell->findData( QString::fromLatin1( example.shell ) ) );
		}
	} );
	connect( m_results, &QTableWidget::itemSelectionChanged, this, &RemoteCommandDialog::showOutput );
	connect( m_plugin, &RemoteCommandFeaturePlugin::resultReceived, this, &RemoteCommandDialog::onResult );
}



void RemoteCommandDialog::runCommand()
{
	const auto script = m_script->toPlainText();
	if( script.trimmed().isEmpty() )
	{
		return;
	}
	if( QMessageBox::question( this, windowTitle(), tr( "Run this command on %n computer(s) now?", nullptr, int( m_computers.size() ) ) )
		!= QMessageBox::Yes )
	{
		return;
	}

	QSettings settings( QStringLiteral("AruniControl"), QStringLiteral("RemoteCommand") );
	settings.setValue( QStringLiteral("script"), script );
	settings.setValue( QStringLiteral("shell"), m_shell->currentData() );
	settings.setValue( QStringLiteral("timeout"), m_timeout->value() );

	m_outputs.clear();
	m_output->clear();
	ComputerControlInterfaceList connected;
	for( int i = 0; i < m_computers.size(); ++i )
	{
		const auto& computer = m_computers.at( i );
		auto item = m_results->item( i, 1 );
		item->setForeground( QBrush() );
		if( computer->state() == ComputerControlInterface::State::Connected )
		{
			item->setText( tr( "Running..." ) );
			connected.append( computer );
		}
		else
		{
			item->setText( tr( "Not connected" ) );
			item->setForeground( QColor( 0xb3, 0x26, 0x1e ) );
		}
	}
	m_job = m_plugin->run( m_shell->currentData().toString(), script, m_timeout->value(), connected );
	m_summary->clear();
}



void RemoteCommandDialog::onResult( ComputerControlInterface::Pointer computer, const QString& job, int exitCode,
									const QString& output, bool timedOut )
{
	const auto index = m_computers.indexOf( computer );
	if( job != m_job || index < 0 )
	{
		return;
	}

	m_outputs[computer.data()] = output;
	auto item = m_results->item( int( index ), 1 );
	const auto firstLine = output.trimmed().section( QLatin1Char('\n'), 0, 0 ).left( 120 );
	if( timedOut )
	{
		item->setText( tr( "Time limit reached" ) );
	}
	else
	{
		item->setText( exitCode == 0 ? ( firstLine.isEmpty() ? tr( "Done" ) : firstLine )
									 : tr( "Exit code %1: %2" ).arg( exitCode ).arg( firstLine ) );
	}
	item->setForeground( exitCode == 0 && timedOut == false ? QColor( 0x1e, 0x8e, 0x3e ) : QColor( 0xb3, 0x26, 0x1e ) );

	int ok = 0;
	for( int i = 0; i < m_computers.size(); ++i )
	{
		ok += m_results->item( i, 1 )->foreground().color() == QColor( 0x1e, 0x8e, 0x3e ) ? 1 : 0;
	}
	m_summary->setText( tr( "%1 of %2 answered, %3 successful" ).arg( m_outputs.size() ).arg( m_computers.size() ).arg( ok ) );

	const auto selection = m_results->selectionModel()->selectedRows();
	if( selection.isEmpty() )
	{
		m_results->selectRow( int( index ) );
	}
	showOutput();
}



void RemoteCommandDialog::showOutput()
{
	const auto selection = m_results->selectionModel()->selectedRows();
	if( selection.isEmpty() )
	{
		return;
	}
	const auto computer = m_computers.value( selection.first().row() );
	m_output->setPlainText( m_outputs.value( computer.data() ) );
}
