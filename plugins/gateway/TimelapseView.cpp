/*
 * TimelapseView.cpp - plays back the scheduled screenshots of a computer
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
#include <QDate>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QUrl>
#include <QVBoxLayout>

#include "MonitoringCollector.h"
#include "TimelapseView.h"


TimelapseView::TimelapseView( QWidget* parent ) :
	QWidget( parent )
{
	auto layout = new QHBoxLayout( this );

	m_computers = new QListWidget;
	m_computers->setMaximumWidth( 220 );
	layout->addWidget( m_computers );

	auto right = new QVBoxLayout;
	auto topRow = new QHBoxLayout;
	topRow->addWidget( new QLabel( tr( "Day" ) ) );
	m_days = new QComboBox;
	topRow->addWidget( m_days, 1 );
	auto folderButton = new QPushButton( tr( "Open folder" ) );
	topRow->addWidget( folderButton );
	right->addLayout( topRow );

	m_image = new QLabel( tr( "No screenshots yet. Turn on scheduled screenshots in the Laptops tab." ) );
	m_image->setAlignment( Qt::AlignCenter );
	m_image->setMinimumSize( 320, 200 );
	m_image->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Ignored );
	m_image->setWordWrap( true );
	m_image->setStyleSheet( QStringLiteral("background: #1e1b18; color: #aaa299; border-radius: 6px;") );
	right->addWidget( m_image, 1 );

	auto controls = new QHBoxLayout;
	m_playButton = new QPushButton( tr( "Play" ) );
	controls->addWidget( m_playButton );
	m_speed = new QComboBox;
	for( const auto fps : { 1, 2, 5, 10 } )
	{
		m_speed->addItem( tr( "%1 frames/s" ).arg( fps ), fps );
	}
	m_speed->setCurrentIndex( 1 );
	controls->addWidget( m_speed );
	m_slider = new QSlider( Qt::Horizontal );
	controls->addWidget( m_slider, 1 );
	m_time = new QLabel;
	m_time->setMinimumWidth( 90 );
	controls->addWidget( m_time );
	right->addLayout( controls );

	layout->addLayout( right, 1 );

	connect( m_computers, &QListWidget::currentRowChanged, this, &TimelapseView::loadComputer );
	connect( m_days, &QComboBox::currentIndexChanged, this, &TimelapseView::loadDay );
	connect( m_slider, &QSlider::valueChanged, this, &TimelapseView::showFrame );
	connect( m_playButton, &QPushButton::clicked, this, &TimelapseView::togglePlayback );
	connect( m_speed, &QComboBox::currentIndexChanged, this, [this]() {
		m_playTimer.setInterval( 1000 / qMax( 1, m_speed->currentData().toInt() ) );
	} );
	connect( folderButton, &QPushButton::clicked, this, [this]() {
		const auto folder = m_folder.isEmpty() ? MonitoringCollector::screenshotDirectory() : m_folder;
		QDir().mkpath( folder );
		QDesktopServices::openUrl( QUrl::fromLocalFile( folder ) );
	} );

	m_playTimer.setInterval( 500 );
	connect( &m_playTimer, &QTimer::timeout, this, [this]() {
		if( m_slider->value() >= m_slider->maximum() )
		{
			togglePlayback();
			return;
		}
		m_slider->setValue( m_slider->value() + 1 );
	} );
}



void TimelapseView::refresh()
{
	const auto current = m_computers->currentItem() ? m_computers->currentItem()->text() : QString{};
	const auto computers = QDir( MonitoringCollector::screenshotDirectory() ).entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name );

	const QSignalBlocker blocker( m_computers );
	m_computers->clear();
	m_computers->addItems( computers );
	const auto matches = m_computers->findItems( current, Qt::MatchExactly );
	m_computers->setCurrentRow( matches.isEmpty() ? ( computers.isEmpty() ? -1 : 0 ) : m_computers->row( matches.first() ) );
	loadComputer();
}



void TimelapseView::loadComputer()
{
	m_days->blockSignals( true );
	m_days->clear();
	if( auto item = m_computers->currentItem() )
	{
		auto days = QDir( QDir( MonitoringCollector::screenshotDirectory() ).filePath( item->text() ) )
						.entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name );
		std::reverse( days.begin(), days.end() );
		for( const auto& day : std::as_const( days ) )
		{
			const auto date = QDate::fromString( day, QStringLiteral("yyyy-MM-dd") );
			m_days->addItem( date.isValid() ? QLocale().toString( date, QLocale::LongFormat ) : day, day );
		}
	}
	m_days->blockSignals( false );
	loadDay();
}



void TimelapseView::loadDay()
{
	m_playTimer.stop();
	m_playButton->setText( tr( "Play" ) );
	m_frames.clear();
	m_folder.clear();

	if( m_computers->currentItem() && m_days->currentIndex() >= 0 )
	{
		m_folder = QDir( MonitoringCollector::screenshotDirectory() )
					   .filePath( m_computers->currentItem()->text() + QLatin1Char('/') + m_days->currentData().toString() );
		m_frames = QDir( m_folder ).entryList( { QStringLiteral("*.jpg") }, QDir::Files, QDir::Name );
	}

	m_slider->blockSignals( true );
	m_slider->setRange( 0, qMax( 0, int( m_frames.size() ) - 1 ) );
	m_slider->setValue( 0 );
	m_slider->blockSignals( false );
	m_slider->setEnabled( m_frames.size() > 1 );
	m_playButton->setEnabled( m_frames.size() > 1 );
	showFrame( 0 );
}



void TimelapseView::showFrame( int index )
{
	if( index < 0 || index >= m_frames.size() )
	{
		m_image->setPixmap( {} );
		m_image->setText( tr( "No screenshots yet. Turn on scheduled screenshots in the Laptops tab." ) );
		m_time->clear();
		return;
	}

	const QPixmap pixmap( QDir( m_folder ).filePath( m_frames.at( index ) ) );
	m_image->setPixmap( pixmap.scaled( m_image->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
	auto time = m_frames.at( index );
	time.chop( 4 );
	m_time->setText( QStringLiteral("%1  (%2/%3)").arg( time.replace( QLatin1Char('-'), QLatin1Char(':') ) )
						 .arg( index + 1 ).arg( m_frames.size() ) );
}



void TimelapseView::togglePlayback()
{
	if( m_playTimer.isActive() )
	{
		m_playTimer.stop();
		m_playButton->setText( tr( "Play" ) );
	}
	else
	{
		if( m_slider->value() >= m_slider->maximum() )
		{
			m_slider->setValue( 0 );
		}
		m_playTimer.setInterval( 1000 / qMax( 1, m_speed->currentData().toInt() ) );
		m_playTimer.start();
		m_playButton->setText( tr( "Pause" ) );
	}
}



void TimelapseView::resizeEvent( QResizeEvent* event )
{
	QWidget::resizeEvent( event );
	showFrame( m_slider->value() );
}
