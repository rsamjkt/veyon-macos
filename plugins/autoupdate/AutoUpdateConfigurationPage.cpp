/*
 * AutoUpdateConfigurationPage.cpp - settings of the automatic updates
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
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "AutoUpdateConfigurationPage.h"
#include "UpdateState.h"
#include "VeyonCore.h"


AutoUpdateConfigurationPage::AutoUpdateConfigurationPage( QWidget* parent ) :
	ConfigurationPage( parent )
{
	setWindowTitle( tr( "Updates" ) );
	setWindowIcon( QIcon( QStringLiteral(":/core/document-save.png") ) );

	auto layout = new QVBoxLayout( this );
	auto box = new QGroupBox( tr( "Automatic updates" ) );
	auto boxLayout = new QVBoxLayout( box );

	auto intro = new QLabel( tr( "New versions of AruniControl are installed automatically: the AruniControl service "
								 "checks the download website every 6 hours, verifies the package and installs it. "
								 "Settings, keys and the Aruni Gateway stay as they are." ) );
	intro->setWordWrap( true );
	boxLayout->addWidget( intro );

	m_enabled = new QCheckBox( tr( "Install updates automatically" ) );
	boxLayout->addWidget( m_enabled );

	auto form = new QFormLayout;
	form->addRow( tr( "Installed version" ), new QLabel( VeyonCore::versionString() ) );
	m_status = new QLabel;
	m_status->setTextFormat( Qt::RichText );
	m_status->setWordWrap( true );
	form->addRow( tr( "Status" ), m_status );
	m_url = new QLineEdit;
	form->addRow( tr( "Update source" ), m_url );
	boxLayout->addLayout( form );

	auto checkButton = new QPushButton( tr( "Check now" ) );
	boxLayout->addWidget( checkButton, 0, Qt::AlignLeft );

	layout->addWidget( box );
	layout->addStretch( 1 );

	connect( checkButton, &QPushButton::clicked, this, [this]() {
		UpdateState::requestCheck();
		m_status->setText( tr( "Checking…" ) );
	} );

	connect( &m_refreshTimer, &QTimer::timeout, this, &AutoUpdateConfigurationPage::refreshStatus );
	m_refreshTimer.start( 2000 );
}



void AutoUpdateConfigurationPage::resetWidgets()
{
	const auto state = UpdateState::load();
	m_enabled->setChecked( state.enabled );
	m_url->setText( state.manifestUrl );
	refreshStatus();
}



void AutoUpdateConfigurationPage::connectWidgetsToProperties()
{
	connect( m_enabled, &QCheckBox::toggled, this, &ConfigurationPage::widgetsChanged );
	connect( m_url, &QLineEdit::textChanged, this, &ConfigurationPage::widgetsChanged );
}



void AutoUpdateConfigurationPage::applyConfiguration()
{
	auto state = UpdateState::load();
	state.enabled = m_enabled->isChecked();
	const auto url = m_url->text().trimmed();
	state.manifestUrl = url.startsWith( QStringLiteral("https://") ) ? url : QString::fromLatin1( UpdateState::DefaultManifestUrl );
	if( state.save() == false )
	{
		QMessageBox::critical( this, tr( "Updates" ),
							   tr( "The settings could not be saved. Please run the Configurator as administrator." ) );
	}
}



void AutoUpdateConfigurationPage::refreshStatus()
{
	const auto status = UpdateState::readStatus();
	const auto state = status[QStringLiteral("state")].toString();
	const auto available = status[QStringLiteral("available")].toString();
	const auto checked = QDateTime::fromString( status[QStringLiteral("checked")].toString(), Qt::ISODate );
	const auto checkedText = checked.isValid() ? QLocale().toString( checked.toLocalTime(), QLocale::ShortFormat ) : QString{};

	QString text;
	QString color = QStringLiteral("#888888");
	if( state == QStringLiteral("current") )
	{
		text = tr( "Up to date (last check: %1)" ).arg( checkedText );
		color = QStringLiteral("#1f9d55");
	}
	else if( state == QStringLiteral("checking") )
	{
		text = tr( "Checking…" );
	}
	else if( state == QStringLiteral("downloading") )
	{
		text = tr( "Downloading version %1…" ).arg( available );
		color = QStringLiteral("#d48e00");
	}
	else if( state == QStringLiteral("installing") )
	{
		text = tr( "Installing version %1…" ).arg( available );
		color = QStringLiteral("#d48e00");
	}
	else if( state == QStringLiteral("error") )
	{
		text = tr( "Problem: %1" ).arg( status[QStringLiteral("error")].toString() );
		color = QStringLiteral("#dc3b3f");
	}
	else
	{
		text = tr( "Not checked yet - the AruniControl service checks a few minutes after start" );
	}

	if( m_status->text().contains( tr( "Checking…" ) ) && state.isEmpty() )
	{
		return;
	}
	m_status->setText( QStringLiteral("<span style='color:%1'>●</span> %2").arg( color, text.toHtmlEscaped() ) );
}
