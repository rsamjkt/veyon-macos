/*
 * ScheduleView.cpp - the schedules in the Configurator
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
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTimeEdit>
#include <QUuid>
#include <QVBoxLayout>

#include "GatewayState.h"
#include "MonitoringCollector.h"
#include "NetworkObjectDirectory.h"
#include "NetworkObjectDirectoryManager.h"
#include "ScheduleView.h"
#include "VeyonCore.h"


namespace {

const auto ScheduleBuiltinDirectoryUid = Plugin::Uid( QStringLiteral("14bacaaa-ebe5-449c-b881-5b382f952571") );

enum ScheduleColumn
{
	ColumnEnabled,
	ColumnName,
	ColumnWhen,
	ColumnAction,
	ColumnRoom,
	ColumnLastRun,
	ColumnCount
};

}



ScheduleView::ScheduleView( QWidget* parent ) :
	QWidget( parent )
{
	auto layout = new QVBoxLayout( this );

	auto intro = new QLabel( tr( "Actions done automatically at set times, e.g. power on the computers in the morning, "
								 "lock the screens during the break or power them down in the evening. This computer "
								 "runs the schedules, so it has to be on at those times." ) );
	intro->setWordWrap( true );
	layout->addWidget( intro );

	m_warning = new QLabel;
	m_warning->setWordWrap( true );
	m_warning->setStyleSheet( QStringLiteral("color: #b3261e;") );
	m_warning->setTextFormat( Qt::PlainText );
	layout->addWidget( m_warning );

	m_table = new QTableWidget( 0, ColumnCount );
	m_table->setHorizontalHeaderLabels( { tr( "On" ), tr( "Name" ), tr( "When" ), tr( "Action" ),
										  tr( "Room" ), tr( "Last run" ) } );
	m_table->setSelectionBehavior( QAbstractItemView::SelectRows );
	m_table->setSelectionMode( QAbstractItemView::SingleSelection );
	m_table->setEditTriggers( QAbstractItemView::NoEditTriggers );
	m_table->verticalHeader()->hide();
	m_table->setMinimumHeight( 160 );
	m_table->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Preferred );
	m_table->horizontalHeader()->setSectionResizeMode( ColumnLastRun, QHeaderView::Stretch );
	layout->addWidget( m_table, 1 );

	auto buttons = new QHBoxLayout;
	auto addButton = new QPushButton( tr( "Add" ) );
	m_editButton = new QPushButton( tr( "Edit" ) );
	m_removeButton = new QPushButton( tr( "Delete" ) );
	m_runButton = new QPushButton( tr( "Run now" ) );
	auto examplesButton = new QPushButton( tr( "Add examples" ) );
	examplesButton->setToolTip( tr( "Power on 07:00, lock screens 12:00-13:00, power down 17:00 (Monday-Friday)" ) );
	buttons->addWidget( addButton );
	buttons->addWidget( m_editButton );
	buttons->addWidget( m_removeButton );
	buttons->addWidget( m_runButton );
	buttons->addStretch( 1 );
	buttons->addWidget( examplesButton );
	layout->addLayout( buttons );

	auto hint = new QLabel( tr( "Power on needs Wake-on-LAN enabled in the BIOS and the MAC addresses in the list of "
								"computers. Power down warns the users 2 minutes before. Results appear in the Activity "
								"tab and, if set up, on Telegram." ) );
	hint->setWordWrap( true );
	hint->setStyleSheet( QStringLiteral("color: gray;") );
	layout->addWidget( hint );

	connect( addButton, &QPushButton::clicked, this, &ScheduleView::addRule );
	connect( m_editButton, &QPushButton::clicked, this, &ScheduleView::editRule );
	connect( m_removeButton, &QPushButton::clicked, this, &ScheduleView::removeRule );
	connect( m_runButton, &QPushButton::clicked, this, &ScheduleView::runRule );
	connect( examplesButton, &QPushButton::clicked, this, &ScheduleView::addExamples );
	connect( m_table, &QTableWidget::cellDoubleClicked, this, &ScheduleView::editRule );
	connect( m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
		const bool selected = selectedRow() >= 0;
		m_editButton->setEnabled( selected );
		m_removeButton->setEnabled( selected );
		m_runButton->setEnabled( selected );
	} );
	connect( m_table, &QTableWidget::itemChanged, this, [this]( QTableWidgetItem* item ) {
		if( item->column() == ColumnEnabled && item->row() < m_schedules.rules.size() )
		{
			const bool enabled = item->checkState() == Qt::Checked;
			if( m_schedules.rules[item->row()].enabled != enabled )
			{
				m_schedules.rules[item->row()].enabled = enabled;
				save();
			}
		}
	} );

	refresh();
}



void ScheduleView::refresh()
{
	m_schedules = Schedules::load();
	const auto status = Schedules::readStatus();
	const auto current = selectedRow();

	const QSignalBlocker blocker( m_table );
	m_table->setRowCount( m_schedules.rules.size() );
	for( int row = 0; row < m_schedules.rules.size(); ++row )
	{
		const auto& rule = m_schedules.rules.at( row );

		auto enabledItem = new QTableWidgetItem;
		enabledItem->setFlags( Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable );
		enabledItem->setCheckState( rule.enabled ? Qt::Checked : Qt::Unchecked );
		m_table->setItem( row, ColumnEnabled, enabledItem );
		m_table->setItem( row, ColumnName, new QTableWidgetItem( rule.name ) );
		m_table->setItem( row, ColumnWhen, new QTableWidgetItem( QStringLiteral("%1  %2").arg( rule.time.toString( QStringLiteral("HH:mm") ),
																								 Schedules::daysText( rule.days ) ) ) );
		m_table->setItem( row, ColumnAction, new QTableWidgetItem( Schedules::actionName( rule.action ) ) );
		m_table->setItem( row, ColumnRoom, new QTableWidgetItem( rule.room.isEmpty() ? tr( "All computers" ) : rule.room ) );

		const auto entry = status[rule.id].toObject();
		const auto time = QDateTime::fromString( entry[QStringLiteral("time")].toString(), Qt::ISODate );
		auto lastRun = new QTableWidgetItem( time.isValid() ?
			QStringLiteral("%1 - %2").arg( time.toString( QStringLiteral("dd/MM HH:mm") ),
										   entry[QStringLiteral("result")].toString() ) : QString{} );
		lastRun->setToolTip( lastRun->text() );
		m_table->setItem( row, ColumnLastRun, lastRun );
	}
	m_table->resizeColumnsToContents();
	m_table->horizontalHeader()->setSectionResizeMode( ColumnLastRun, QHeaderView::Stretch );
	if( current >= 0 && current < m_table->rowCount() )
	{
		m_table->selectRow( current );
	}

	const bool selected = selectedRow() >= 0;
	m_editButton->setEnabled( selected );
	m_removeButton->setEnabled( selected );
	m_runButton->setEnabled( selected );

	const bool needsKey = std::any_of( m_schedules.rules.cbegin(), m_schedules.rules.cend(), []( const Schedules::Rule& rule ) {
		return rule.enabled && rule.action != Schedules::Action::PowerOn;
	} );
	m_warning->setVisible( needsKey && MonitoringCollector::availableKeyName( GatewayState::load().sharedKeyName ).isEmpty() );
	m_warning->setText( tr( "This computer has no private authentication key, so it cannot connect to the computers. "
							"Create or import one on the \"Authentication keys\" page." ) );
}



void ScheduleView::save()
{
	if( m_schedules.save() == false )
	{
		QMessageBox::critical( this, tr( "Schedules" ), tr( "Could not save the schedules (run as administrator)." ) );
	}
	refresh();
}



void ScheduleView::addRule()
{
	Schedules::Rule rule;
	rule.id = QUuid::createUuid().toString( QUuid::WithoutBraces );
	ScheduleRuleDialog dialog( rule, this );
	if( dialog.exec() == QDialog::Accepted )
	{
		m_schedules.rules.append( dialog.rule() );
		save();
	}
}



void ScheduleView::editRule()
{
	const auto row = selectedRow();
	if( row < 0 )
	{
		return;
	}

	ScheduleRuleDialog dialog( m_schedules.rules.at( row ), this );
	if( dialog.exec() == QDialog::Accepted )
	{
		m_schedules.rules[row] = dialog.rule();
		save();
	}
}



void ScheduleView::removeRule()
{
	const auto row = selectedRow();
	if( row < 0 ||
		QMessageBox::question( this, tr( "Schedules" ), tr( "Delete the schedule \"%1\"?" ).arg( m_schedules.rules.at( row ).name ) ) != QMessageBox::Yes )
	{
		return;
	}

	m_schedules.rules.removeAt( row );
	save();
}



void ScheduleView::runRule()
{
	const auto row = selectedRow();
	if( row < 0 )
	{
		return;
	}

	const auto& rule = m_schedules.rules.at( row );
	if( QMessageBox::question( this, tr( "Schedules" ),
							   tr( "Run \"%1\" (%2) now?" ).arg( rule.name, Schedules::actionName( rule.action ) ) ) != QMessageBox::Yes )
	{
		return;
	}

	Schedules::requestRun( rule.id );
	QMessageBox::information( this, tr( "Schedules" ),
							  tr( "The schedule runs within half a minute - the result appears in the list." ) );
}



void ScheduleView::addExamples()
{
	const auto make = []( const QString& name, int hour, int minute, Schedules::Action action ) {
		Schedules::Rule rule;
		rule.id = QUuid::createUuid().toString( QUuid::WithoutBraces );
		rule.name = name;
		rule.time = QTime( hour, minute );
		rule.action = action;
		rule.days = 0x1f;
		return rule;
	};

	m_schedules.rules.append( make( tr( "Morning" ), 7, 0, Schedules::Action::PowerOn ) );
	m_schedules.rules.append( make( tr( "Break" ), 12, 0, Schedules::Action::LockScreen ) );
	m_schedules.rules.append( make( tr( "Break over" ), 13, 0, Schedules::Action::UnlockScreen ) );
	m_schedules.rules.append( make( tr( "Closing time" ), 17, 0, Schedules::Action::PowerDown ) );
	save();

	QMessageBox::information( this, tr( "Schedules" ),
							  tr( "Four example schedules were added. Adjust the times with \"Edit\" or switch off the "
								  "ones you do not need." ) );
}



int ScheduleView::selectedRow() const
{
	const auto rows = m_table->selectionModel()->selectedRows();
	return rows.isEmpty() ? -1 : rows.first().row();
}



ScheduleRuleDialog::ScheduleRuleDialog( const Schedules::Rule& rule, QWidget* parent ) :
	QDialog( parent ),
	m_rule( rule )
{
	setWindowTitle( tr( "Schedule" ) );
	resize( 480, 520 );

	auto layout = new QVBoxLayout( this );
	auto form = new QFormLayout;
	layout->addLayout( form );

	m_name = new QLineEdit( rule.name );
	m_name->setPlaceholderText( tr( "e.g. Closing time" ) );
	form->addRow( tr( "Name" ), m_name );

	m_action = new QComboBox;
	for( const auto action : Schedules::actions() )
	{
		m_action->addItem( Schedules::actionName( action ), static_cast<int>( action ) );
	}
	m_action->setCurrentIndex( m_action->findData( static_cast<int>( rule.action ) ) );
	form->addRow( tr( "Action" ), m_action );

	m_time = new QTimeEdit( rule.time );
	m_time->setDisplayFormat( QStringLiteral("HH:mm") );
	form->addRow( tr( "Time" ), m_time );

	auto daysLayout = new QHBoxLayout;
	for( int day = 1; day <= 7; ++day )
	{
		auto checkBox = new QCheckBox( QLocale().dayName( day, QLocale::ShortFormat ) );
		checkBox->setChecked( rule.days & ( 1 << ( day - 1 ) ) );
		daysLayout->addWidget( checkBox );
		m_days.append( checkBox );
	}
	form->addRow( tr( "Days" ), daysLayout );

	m_room = new QComboBox;
	m_room->addItem( tr( "All computers" ), QString{} );
	const auto allRooms = rooms();
	for( const auto& room : allRooms )
	{
		m_room->addItem( room, room );
	}
	if( rule.room.isEmpty() == false && m_room->findData( rule.room ) < 0 )
	{
		m_room->addItem( rule.room, rule.room );
	}
	m_room->setCurrentIndex( qMax( 0, m_room->findData( rule.room ) ) );
	form->addRow( tr( "Room" ), m_room );

	m_textLabel = new QLabel;
	m_text = new QLineEdit( rule.text );
	form->addRow( m_textLabel, m_text );

	m_sitesLabel = new QLabel;
	m_sites = new QPlainTextEdit( rule.sites.join( QLatin1Char('\n') ) );
	m_sites->setPlaceholderText( tr( "One website per line" ) );
	form->addRow( m_sitesLabel, m_sites );

	m_enabled = new QCheckBox( tr( "Schedule is active" ) );
	m_enabled->setChecked( rule.enabled );
	layout->addWidget( m_enabled );

	auto buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel );
	connect( buttons, &QDialogButtonBox::accepted, this, &QDialog::accept );
	connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
	layout->addWidget( buttons );

	connect( m_action, &QComboBox::currentIndexChanged, this, &ScheduleRuleDialog::updateFields );
	updateFields();
}



Schedules::Rule ScheduleRuleDialog::rule() const
{
	auto rule = m_rule;
	rule.action = static_cast<Schedules::Action>( m_action->currentData().toInt() );
	rule.name = m_name->text().trimmed();
	if( rule.name.isEmpty() )
	{
		rule.name = Schedules::actionName( rule.action );
	}
	rule.enabled = m_enabled->isChecked();
	rule.time = m_time->time();
	rule.days = 0;
	for( int i = 0; i < m_days.size(); ++i )
	{
		if( m_days.at( i )->isChecked() )
		{
			rule.days |= 1 << i;
		}
	}
	rule.room = m_room->currentData().toString();

	const bool usesText = rule.action == Schedules::Action::Message || rule.action == Schedules::Action::StartExam;
	rule.text = usesText ? m_text->text().trimmed() : QString{};
	rule.sites.clear();
	if( Schedules::needsSites( rule.action ) )
	{
		const auto lines = m_sites->toPlainText().split( QLatin1Char('\n') );
		for( const auto& line : lines )
		{
			if( line.trimmed().isEmpty() == false )
			{
				rule.sites.append( line.trimmed() );
			}
		}
	}
	return rule;
}



void ScheduleRuleDialog::accept()
{
	const auto current = rule();
	if( current.days == 0 )
	{
		QMessageBox::warning( this, windowTitle(), tr( "Choose at least one day." ) );
		return;
	}
	if( current.action == Schedules::Action::Message && current.text.isEmpty() )
	{
		QMessageBox::warning( this, windowTitle(), tr( "Enter the message." ) );
		return;
	}
	if( current.action == Schedules::Action::BlockSites && current.sites.isEmpty() )
	{
		QMessageBox::warning( this, windowTitle(), tr( "Enter the websites to block." ) );
		return;
	}
	QDialog::accept();
}



QStringList ScheduleRuleDialog::rooms()
{
	QStringList result;
	auto directory = VeyonCore::networkObjectDirectoryManager().createDirectory( ScheduleBuiltinDirectoryUid, nullptr );
	if( directory )
	{
		directory->update();
		const auto locations = directory->queryObjects( NetworkObject::Type::Location, NetworkObject::Attribute::None, {} );
		for( const auto& location : locations )
		{
			if( location.name().isEmpty() == false && result.contains( location.name() ) == false )
			{
				result.append( location.name() );
			}
		}
		delete directory;
	}
	result.sort( Qt::CaseInsensitive );
	return result;
}



void ScheduleRuleDialog::updateFields()
{
	const auto action = static_cast<Schedules::Action>( m_action->currentData().toInt() );

	const bool sites = Schedules::needsSites( action );
	m_sitesLabel->setVisible( sites );
	m_sites->setVisible( sites );
	m_sitesLabel->setText( action == Schedules::Action::StartExam ? tr( "Exam websites\n(the only ones reachable)" )
																 : tr( "Websites to block" ) );

	const bool text = action == Schedules::Action::Message || action == Schedules::Action::StartExam;
	m_textLabel->setVisible( text );
	m_text->setVisible( text );
	m_textLabel->setText( action == Schedules::Action::StartExam ? tr( "Open website" ) : tr( "Message" ) );
	m_text->setPlaceholderText( action == Schedules::Action::StartExam ? QStringLiteral("https://cbt.sekolah.sch.id")
																	   : tr( "e.g. 10 minutes until closing time - please save your work." ) );
}
