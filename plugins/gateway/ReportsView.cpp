/*
 * ReportsView.cpp - reports tab in the Configurator
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
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTimeEdit>
#include <QVBoxLayout>

#include "GatewayState.h"
#include "ReportsView.h"


namespace {

QString reportsDefaultPath( const QString& name )
{
	return QDir( QStandardPaths::writableLocation( QStandardPaths::DocumentsLocation ) ).filePath( name );
}

}



ReportsView::ReportsView( QWidget* parent ) :
	QWidget( parent )
{
	auto layout = new QVBoxLayout( this );
	layout->setContentsMargins( 0, 0, 0, 0 );
	auto tabs = new QTabWidget;
	tabs->addTab( createAttendanceTab(), tr( "Attendance" ) );
	tabs->addTab( createUsageTab(), tr( "Application usage" ) );
	tabs->addTab( createDailyReportTab(), tr( "Daily report" ) );
	layout->addWidget( tabs );
}



QComboBox* ReportsView::createRangeCombo()
{
	auto combo = new QComboBox;
	combo->addItem( tr( "Today" ), 0 );
	combo->addItem( tr( "Yesterday" ), -1 );
	combo->addItem( tr( "Last 7 days" ), 7 );
	combo->addItem( tr( "Last 30 days" ), 30 );
	return combo;
}



QPair<QDate, QDate> ReportsView::range( const QComboBox* combo )
{
	const auto today = QDate::currentDate();
	const auto value = combo->currentData().toInt();
	if( value == 0 )
	{
		return { today, today };
	}
	if( value == -1 )
	{
		return { today.addDays( -1 ), today.addDays( -1 ) };
	}
	return { today.addDays( -( value - 1 ) ), today };
}



QWidget* ReportsView::createAttendanceTab()
{
	auto page = new QWidget;
	auto layout = new QVBoxLayout( page );

	auto intro = new QLabel( tr( "Who logged on to which computer and for how long - recorded by every computer "
								 "with AruniControl 1.8 or newer, collected by the gateway every 15 minutes." ) );
	intro->setWordWrap( true );
	layout->addWidget( intro );

	auto row = new QHBoxLayout;
	m_attendanceRange = createRangeCombo();
	row->addWidget( m_attendanceRange );
	row->addStretch( 1 );
	auto exportButton = new QPushButton( tr( "Export..." ) );
	row->addWidget( exportButton );
	layout->addLayout( row );

	m_attendance = new QTableWidget( 0, 6 );
	m_attendance->setHorizontalHeaderLabels( { tr( "Date" ), tr( "User" ), tr( "Computer" ), tr( "Logon" ), tr( "Logoff" ), tr( "Duration" ) } );
	m_attendance->setEditTriggers( QAbstractItemView::NoEditTriggers );
	m_attendance->setSelectionBehavior( QAbstractItemView::SelectRows );
	m_attendance->setSortingEnabled( true );
	m_attendance->verticalHeader()->hide();
	m_attendance->horizontalHeader()->setStretchLastSection( true );
	m_attendance->setMinimumHeight( 180 );
	layout->addWidget( m_attendance, 1 );

	m_attendanceSummary = new QLabel;
	m_attendanceSummary->setStyleSheet( QStringLiteral("color: gray;") );
	layout->addWidget( m_attendanceSummary );

	connect( m_attendanceRange, &QComboBox::currentIndexChanged, this, &ReportsView::refreshAttendance );
	connect( exportButton, &QPushButton::clicked, this, [this]() {
		const auto fileName = QFileDialog::getSaveFileName( this, tr( "Export attendance" ), reportsDefaultPath( QStringLiteral("absensi.csv") ),
															tr( "CSV files (*.csv)" ) );
		if( fileName.isEmpty() == false && Reports::exportAttendanceCsv( fileName, m_sessions ) == false )
		{
			QMessageBox::critical( this, tr( "Reports" ), tr( "Cannot write %1" ).arg( fileName ) );
		}
	} );
	return page;
}



QWidget* ReportsView::createUsageTab()
{
	auto page = new QWidget;
	auto layout = new QVBoxLayout( page );

	auto intro = new QLabel( tr( "How long each application was in use (in the foreground, without idle time). Only "
								 "application names are recorded - no window titles and no typing." ) );
	intro->setWordWrap( true );
	layout->addWidget( intro );

	auto row = new QHBoxLayout;
	m_usageRange = createRangeCombo();
	m_usageRange->removeItem( 3 );	// the inventory keeps 8 days
	row->addWidget( m_usageRange );
	m_usageComputer = new QComboBox;
	m_usageComputer->setMinimumWidth( 180 );
	row->addWidget( m_usageComputer );
	row->addStretch( 1 );
	auto exportButton = new QPushButton( tr( "Export..." ) );
	row->addWidget( exportButton );
	layout->addLayout( row );

	m_usageTable = new QTableWidget( 0, 4 );
	m_usageTable->setHorizontalHeaderLabels( { tr( "Application" ), tr( "Time" ), tr( "Computers" ), tr( "Users" ) } );
	m_usageTable->setEditTriggers( QAbstractItemView::NoEditTriggers );
	m_usageTable->setSelectionBehavior( QAbstractItemView::SelectRows );
	m_usageTable->verticalHeader()->hide();
	m_usageTable->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::Stretch );
	m_usageTable->setMinimumHeight( 180 );
	layout->addWidget( m_usageTable, 1 );

	m_usageSummary = new QLabel;
	m_usageSummary->setWordWrap( true );
	m_usageSummary->setStyleSheet( QStringLiteral("color: gray;") );
	layout->addWidget( m_usageSummary );

	connect( m_usageRange, &QComboBox::currentIndexChanged, this, &ReportsView::refreshUsage );
	connect( m_usageComputer, &QComboBox::currentIndexChanged, this, &ReportsView::refreshUsage );
	connect( exportButton, &QPushButton::clicked, this, [this]() {
		const auto fileName = QFileDialog::getSaveFileName( this, tr( "Export application usage" ),
															reportsDefaultPath( QStringLiteral("pemakaian-aplikasi.csv") ), tr( "CSV files (*.csv)" ) );
		if( fileName.isEmpty() == false && Reports::exportUsageCsv( fileName, m_usage ) == false )
		{
			QMessageBox::critical( this, tr( "Reports" ), tr( "Cannot write %1" ).arg( fileName ) );
		}
	} );
	return page;
}



QWidget* ReportsView::createDailyReportTab()
{
	auto page = new QWidget;
	auto layout = new QVBoxLayout( page );

	auto row = new QHBoxLayout;
	m_reportEnabled = new QCheckBox( tr( "Send a daily report to Telegram at" ) );
	row->addWidget( m_reportEnabled );
	m_reportTime = new QTimeEdit;
	m_reportTime->setDisplayFormat( QStringLiteral("HH:mm") );
	row->addWidget( m_reportTime );
	row->addStretch( 1 );
	auto sendButton = new QPushButton( tr( "Send now" ) );
	row->addWidget( sendButton );
	layout->addLayout( row );

	auto hint = new QLabel( tr( "Computers online, disks almost full, computers not seen for days, schedules, attendance, "
								"refused accesses and the most used applications of the day. Set up the Telegram bot "
								"on the \"Notifications\" tab first." ) );
	hint->setWordWrap( true );
	hint->setStyleSheet( QStringLiteral("color: gray;") );
	layout->addWidget( hint );

	layout->addWidget( new QLabel( tr( "Preview of today:" ) ) );
	m_reportPreview = new QTextBrowser;
	layout->addWidget( m_reportPreview, 1 );

	connect( m_reportEnabled, &QCheckBox::toggled, this, &ReportsView::saveDailyReportSettings );
	connect( m_reportTime, &QTimeEdit::timeChanged, this, &ReportsView::saveDailyReportSettings );
	connect( sendButton, &QPushButton::clicked, this, [this]() {
		if( GatewayState::load().isTelegramConfigured() == false )
		{
			QMessageBox::warning( this, tr( "Reports" ), tr( "Set up the Telegram bot on the \"Notifications\" tab first." ) );
			return;
		}
		Reports::requestDailyReport();
		QMessageBox::information( this, tr( "Reports" ), tr( "The report is sent within a minute." ) );
	} );
	return page;
}



void ReportsView::refresh()
{
	refreshAttendance();

	const auto current = m_usageComputer->currentData().toString();
	const QSignalBlocker blocker( m_usageComputer );
	m_usageComputer->clear();
	m_usageComputer->addItem( tr( "All computers" ), QString{} );
	const auto records = InventoryStore::list();
	for( const auto& record : records )
	{
		m_usageComputer->addItem( record.name, record.name );
	}
	m_usageComputer->setCurrentIndex( qMax( 0, m_usageComputer->findData( current ) ) );
	refreshUsage();

	refreshDailyReport();
}



void ReportsView::refreshAttendance()
{
	const auto dates = range( m_attendanceRange );
	m_sessions = Reports::attendance( dates.first, dates.second );

	m_attendance->setSortingEnabled( false );
	m_attendance->setRowCount( int( m_sessions.size() ) );
	QSet<QString> users;
	qint64 total = 0;
	for( int row = 0; row < m_sessions.size(); ++row )
	{
		const auto& session = m_sessions.at( row );
		const auto start = session.start.toLocalTime();
		users.insert( session.user );
		total += session.seconds();
		m_attendance->setItem( row, 0, new QTableWidgetItem( start.date().toString( Qt::ISODate ) ) );
		m_attendance->setItem( row, 1, new QTableWidgetItem( session.user ) );
		m_attendance->setItem( row, 2, new QTableWidgetItem( session.computer ) );
		m_attendance->setItem( row, 3, new QTableWidgetItem( start.time().toString( QStringLiteral("HH:mm") ) ) );
		m_attendance->setItem( row, 4, new QTableWidgetItem( session.end.isValid() ? session.end.toLocalTime().toString( QStringLiteral("HH:mm") )
																				   : ( session.open ? tr( "still logged on" ) : tr( "unknown" ) ) ) );
		auto durationItem = new QTableWidgetItem( session.end.isValid() ? Reports::duration( session.seconds() ) : QString{} );
		durationItem->setData( Qt::UserRole, session.seconds() );
		m_attendance->setItem( row, 5, durationItem );
	}
	m_attendance->setSortingEnabled( true );
	m_attendance->resizeColumnsToContents();
	m_attendance->horizontalHeader()->setStretchLastSection( true );
	m_attendanceSummary->setText( m_sessions.isEmpty() ? tr( "No logons in this period." )
													   : tr( "%1 sessions of %2 users, %3 in total" )
															 .arg( m_sessions.size() ).arg( users.size() ).arg( Reports::duration( total ) ) );
}



void ReportsView::refreshUsage()
{
	const auto dates = range( m_usageRange );
	const auto computer = m_usageComputer->currentData().toString();
	m_usage.clear();
	const auto all = Reports::usage( dates.first, dates.second );
	for( const auto& entry : all )
	{
		if( computer.isEmpty() || entry.computer == computer )
		{
			m_usage.append( entry );
		}
	}

	struct Total { int seconds{0}; QSet<QString> computers; QSet<QString> users; };
	QMap<QString, Total> totals;
	qint64 sum = 0;
	for( const auto& entry : std::as_const( m_usage ) )
	{
		auto& total = totals[entry.application];
		total.seconds += entry.seconds;
		total.computers.insert( entry.computer );
		total.users.insert( entry.user );
		sum += entry.seconds;
	}

	QList<QPair<QString, Total>> sorted;
	for( auto it = totals.cbegin(); it != totals.cend(); ++it )
	{
		sorted.append( { it.key(), it.value() } );
	}
	std::sort( sorted.begin(), sorted.end(), []( const QPair<QString, Total>& a, const QPair<QString, Total>& b ) {
		return a.second.seconds > b.second.seconds;
	} );

	m_usageTable->setRowCount( int( sorted.size() ) );
	for( int row = 0; row < sorted.size(); ++row )
	{
		const auto& entry = sorted.at( row );
		m_usageTable->setItem( row, 0, new QTableWidgetItem( entry.first ) );
		m_usageTable->setItem( row, 1, new QTableWidgetItem( Reports::duration( entry.second.seconds ) ) );
		m_usageTable->setItem( row, 2, new QTableWidgetItem( QString::number( entry.second.computers.size() ) ) );
		auto users = entry.second.users.values();
		users.sort();
		m_usageTable->setItem( row, 3, new QTableWidgetItem( users.join( QStringLiteral(", ") ) ) );
	}
	m_usageTable->resizeColumnsToContents();
	m_usageTable->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::Stretch );
	m_usageSummary->setText( m_usage.isEmpty() ? tr( "No usage recorded in this period. It is collected with the inventory "
													 "(every 6 hours) from computers with AruniControl 1.8 or newer." )
											   : tr( "%1 in total" ).arg( Reports::duration( sum ) ) );
}



void ReportsView::refreshDailyReport()
{
	const auto settings = Reports::loadDailyReportSettings();
	m_loading = true;
	m_reportEnabled->setChecked( settings.enabled );
	m_reportTime->setTime( settings.time );
	m_loading = false;
	m_reportPreview->setPlainText( Reports::dailyReport( QDate::currentDate() ) );
}



void ReportsView::saveDailyReportSettings()
{
	if( m_loading )
	{
		return;
	}
	auto settings = Reports::loadDailyReportSettings();
	settings.enabled = m_reportEnabled->isChecked();
	settings.time = m_reportTime->time();
	Reports::saveDailyReportSettings( settings );
}
