/*
 * SiteFilterDialog.cpp - choose the websites to block
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
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "SiteFilterDialog.h"
#include "SiteFilterFeaturePlugin.h"


SiteFilterDialog::SiteFilterDialog( SiteFilterFeaturePlugin* plugin, const ComputerControlInterfaceList& computers,
									QWidget* parent ) :
	QDialog( parent )
{
	setWindowTitle( tr( "Block websites" ) );
	resize( 520, 560 );

	auto layout = new QVBoxLayout( this );

	auto intro = new QLabel( computers.size() == 1 ?
		tr( "Websites blocked on %1:" ).arg( computers.first()->computer().displayName() ) :
		tr( "Websites blocked on the %1 selected computers:" ).arg( computers.size() ) );
	intro->setWordWrap( true );
	layout->addWidget( intro );

	auto presetsBox = new QGroupBox( tr( "Quick selection" ) );
	auto presetsLayout = new QGridLayout( presetsBox );
	const auto presets = SiteFilterFeaturePlugin::presets();
	for( int i = 0; i < presets.size(); ++i )
	{
		auto checkBox = new QCheckBox( presets.at( i ).first );
		checkBox->setToolTip( presets.at( i ).second.join( QStringLiteral(", ") ) );
		presetsLayout->addWidget( checkBox, i / 2, i % 2 );
		m_presets.append( { checkBox, presets.at( i ).second } );

		connect( checkBox, &QCheckBox::toggled, this, [this, domains = presets.at( i ).second]( bool on ) {
			auto current = sites();
			for( const auto& domain : domains )
			{
				if( on && current.contains( domain ) == false )
				{
					current.append( domain );
				}
				else if( on == false )
				{
					current.removeAll( domain );
				}
			}
			setSites( current );
			m_edited = true;
		} );
	}
	layout->addWidget( presetsBox );

	auto listLabel = new QLabel( tr( "One website per line (e.g. youtube.com). Subdomains www. and m. are blocked as well." ) );
	listLabel->setWordWrap( true );
	layout->addWidget( listLabel );

	m_sitesEdit = new QPlainTextEdit;
	m_sitesEdit->setPlaceholderText( QStringLiteral("youtube.com\nfacebook.com") );
	layout->addWidget( m_sitesEdit, 1 );
	connect( m_sitesEdit, &QPlainTextEdit::textChanged, this, [this]() {
		m_edited = true;
		updatePresetStates();
	} );

	m_statusLabel = new QLabel;
	m_statusLabel->setWordWrap( true );
	layout->addWidget( m_statusLabel );

	auto buttons = new QDialogButtonBox( QDialogButtonBox::Cancel );
	auto blockButton = buttons->addButton( tr( "Block" ), QDialogButtonBox::AcceptRole );
	auto clearButton = buttons->addButton( tr( "Unblock all" ), QDialogButtonBox::DestructiveRole );
	blockButton->setDefault( true );
	connect( blockButton, &QPushButton::clicked, this, &QDialog::accept );
	connect( clearButton, &QPushButton::clicked, this, [this]() {
		m_clearAll = true;
		accept();
	} );
	connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
	layout->addWidget( buttons );

	// the lists the computers report (answer to the query sent before)
	setSites( plugin->sitesOf( computers.first() ) );
	m_edited = false;
	connect( plugin, &SiteFilterFeaturePlugin::statusReceived, this,
			 [this]( const ComputerControlInterface::Pointer& computer, const QStringList& reported, bool supported, const QString& error ) {
		if( m_edited == false )
		{
			auto current = sites();
			for( const auto& site : reported )
			{
				if( current.contains( site ) == false )
				{
					current.append( site );
				}
			}
			setSites( current );
			m_edited = false;
		}
		if( supported == false || error.isEmpty() == false )
		{
			m_errors.append( QStringLiteral("%1: %2").arg( computer->computer().displayName(),
														   supported ? error : tr( "not supported" ) ) );
			m_statusLabel->setText( m_errors.mid( 0, 5 ).join( QLatin1Char('\n') ) );
		}
	} );
}



QStringList SiteFilterDialog::sites() const
{
	if( m_clearAll )
	{
		return {};
	}
	return SiteFilterFeaturePlugin::normalizedDomains( m_sitesEdit->toPlainText().split( QLatin1Char('\n') ) );
}



void SiteFilterDialog::setSites( const QStringList& sites )
{
	const QSignalBlocker blocker( m_sitesEdit );
	m_sitesEdit->setPlainText( sites.join( QLatin1Char('\n') ) );
	updatePresetStates();
}



void SiteFilterDialog::updatePresetStates()
{
	const auto current = sites();
	for( const auto& preset : std::as_const( m_presets ) )
	{
		const QSignalBlocker blocker( preset.first );
		preset.first->setChecked( std::all_of( preset.second.cbegin(), preset.second.cend(),
											   [&current]( const QString& domain ) { return current.contains( domain ); } ) );
	}
}
