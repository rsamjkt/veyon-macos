/*
 * GatewayConfigurationPage.cpp - Configurator page of the Aruni Gateway
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
#include <QComboBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "qrcodegen.hpp"

#include "GatewayConfigurationPage.h"
#include "Filesystem.h"
#include "GatewayState.h"
#include "VeyonConfiguration.h"
#include "VeyonCore.h"


GatewayConfigurationPage::GatewayConfigurationPage( QWidget* parent ) :
	ConfigurationPage( parent )
{
	setWindowTitle( tr( "Aruni Gateway" ) );
	setWindowIcon( QIcon( QStringLiteral(":/core/application-x-pem-key.png") ) );

	auto layout = new QVBoxLayout( this );

	// --- general
	auto generalBox = new QGroupBox( tr( "Access from the AruniControl app over the internet" ) );
	auto generalLayout = new QVBoxLayout( generalBox );

	auto intro = new QLabel( tr( "Turn this computer into a gateway for this network: teachers can then reach all computers "
								 "of the lab/office from the AruniControl app - also on mobile data - without VPN or "
								 "router configuration. This computer has to stay switched on. Access to the computers "
								 "still requires the authentication keys." ) );
	intro->setWordWrap( true );
	generalLayout->addWidget( intro );

	m_enabled = new QCheckBox( tr( "Use this computer as Aruni Gateway" ) );
	generalLayout->addWidget( m_enabled );

	auto form = new QFormLayout;
	m_siteName = new QLineEdit;
	m_siteName->setPlaceholderText( tr( "e.g. Computer lab 1" ) );
	form->addRow( tr( "Location name (shown in the app)" ), m_siteName );
	m_relayUrl = new QLineEdit;
	form->addRow( tr( "Relay server" ), m_relayUrl );
	m_status = new QLabel;
	m_status->setTextFormat( Qt::RichText );
	form->addRow( tr( "Status" ), m_status );
	m_gatewayId = new QLabel;
	m_gatewayId->setTextInteractionFlags( Qt::TextSelectableByMouse );
	form->addRow( tr( "Gateway ID" ), m_gatewayId );
	generalLayout->addLayout( form );

	layout->addWidget( generalBox );

	// --- pairing
	auto pairBox = new QGroupBox( tr( "Connect a phone" ) );
	auto pairLayout = new QVBoxLayout( pairBox );
	auto pairIntro = new QLabel( tr( "Open the AruniControl app, choose \"Scan QR code\" and scan the code below. "
									 "Each code works once and expires after 15 minutes." ) );
	pairIntro->setWordWrap( true );
	pairLayout->addWidget( pairIntro );

	m_pairButton = new QPushButton( tr( "Show pairing QR code" ) );
	auto keyRow = new QHBoxLayout;
	keyRow->addWidget( new QLabel( tr( "Give new phones this access key" ) ) );
	m_sharedKey = new QComboBox;
	keyRow->addWidget( m_sharedKey, 1 );
	pairLayout->addLayout( keyRow );
	auto keyHint = new QLabel( tr( "With an access key selected, scanning the QR code is all a teacher has to do - "
								   "otherwise the private key has to be imported in the app separately." ) );
	keyHint->setWordWrap( true );
	pairLayout->addWidget( keyHint );

	pairLayout->addWidget( m_pairButton, 0, Qt::AlignLeft );

	m_pairingBox = new QWidget;
	auto pairingBoxLayout = new QHBoxLayout( m_pairingBox );
	pairingBoxLayout->setContentsMargins( 0, 0, 0, 0 );
	m_qrCode = new QLabel;
	pairingBoxLayout->addWidget( m_qrCode );
	auto codeColumn = new QVBoxLayout;
	m_pairingHint = new QLabel;
	m_pairingHint->setWordWrap( true );
	codeColumn->addWidget( m_pairingHint );
	m_pairingCode = new QLineEdit;
	m_pairingCode->setReadOnly( true );
	codeColumn->addWidget( m_pairingCode );
	auto copyButton = new QPushButton( tr( "Copy link (e.g. to send via WhatsApp)" ) );
	codeColumn->addWidget( copyButton, 0, Qt::AlignLeft );
	codeColumn->addStretch();
	pairingBoxLayout->addLayout( codeColumn, 1 );
	m_pairingBox->hide();
	pairLayout->addWidget( m_pairingBox );

	layout->addWidget( pairBox );

	// --- devices
	auto devicesBox = new QGroupBox( tr( "Connected phones" ) );
	auto devicesLayout = new QVBoxLayout( devicesBox );
	m_devices = new QTableWidget( 0, 3 );
	m_devices->setHorizontalHeaderLabels( { tr( "Device" ), tr( "Connected since" ), tr( "Last used" ) } );
	m_devices->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::Stretch );
	m_devices->verticalHeader()->hide();
	m_devices->setSelectionBehavior( QAbstractItemView::SelectRows );
	m_devices->setEditTriggers( QAbstractItemView::NoEditTriggers );
	devicesLayout->addWidget( m_devices );
	m_removeButton = new QPushButton( tr( "Remove access" ) );
	devicesLayout->addWidget( m_removeButton, 0, Qt::AlignLeft );
	layout->addWidget( devicesBox, 1 );

	connect( m_pairButton, &QPushButton::clicked, this, &GatewayConfigurationPage::startPairing );
	connect( copyButton, &QPushButton::clicked, this, [this]() {
		QApplication::clipboard()->setText( QStringLiteral("arunicontrol://pair?c=") +
											QString::fromUtf8( QUrl::toPercentEncoding( m_pairingCode->text() ) ) );
	} );
	connect( m_removeButton, &QPushButton::clicked, this, &GatewayConfigurationPage::removeSelectedDevice );

	// changes of the switch apply right away - the gateway picks them up
	connect( m_enabled, &QCheckBox::toggled, this, [this]() { applyConfiguration(); } );

	connect( &m_refreshTimer, &QTimer::timeout, this, [this]() {
		refreshStatus();
		refreshDevices();
	} );
	m_refreshTimer.start( 2000 );
}



void GatewayConfigurationPage::resetWidgets()
{
	// creates and stores the identity on first use
	GatewayState::update( []( GatewayState& ) {} );
	const auto state = GatewayState::load();

	const QSignalBlocker blocker( m_enabled );
	m_enabled->setChecked( state.enabled );
	m_siteName->setText( state.siteName );
	m_relayUrl->setText( state.relayUrl );
	m_gatewayId->setText( state.gatewayId );

	// private keys available on this computer
	m_sharedKey->clear();
	m_sharedKey->addItem( tr( "none (import the key in the app)" ), QString{} );
	const auto keyDirectory = QDir( VeyonCore::filesystem().expandPath( VeyonCore::config().privateKeyBaseDir() ) );
	const auto keyNames = keyDirectory.entryList( QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name );
	for( const auto& keyName : keyNames )
	{
		if( QFileInfo::exists( VeyonCore::filesystem().privateKeyPath( keyName ) ) )
		{
			m_sharedKey->addItem( keyName, keyName );
		}
	}
	m_sharedKey->setCurrentIndex( qMax( 0, m_sharedKey->findData( state.sharedKeyName ) ) );

	refreshStatus();
	refreshDevices();
}



void GatewayConfigurationPage::connectWidgetsToProperties()
{
	connect( m_siteName, &QLineEdit::textChanged, this, &ConfigurationPage::widgetsChanged );
	connect( m_relayUrl, &QLineEdit::textChanged, this, &ConfigurationPage::widgetsChanged );
	connect( m_sharedKey, &QComboBox::currentIndexChanged, this, &ConfigurationPage::widgetsChanged );
}



void GatewayConfigurationPage::applyConfiguration()
{
	const auto enabled = m_enabled->isChecked();
	const auto siteName = m_siteName->text().trimmed();
	const auto relayUrl = m_relayUrl->text().trimmed();
	const auto sharedKey = m_sharedKey->currentData().toString();

	if( GatewayState::update( [=]( GatewayState& state ) {
			state.enabled = enabled;
			if( siteName.isEmpty() == false )
			{
				state.siteName = siteName;
			}
			if( relayUrl.startsWith( QStringLiteral("ws") ) )
			{
				state.relayUrl = relayUrl;
			}
			state.sharedKeyName = sharedKey;
		} ) == false )
	{
		QMessageBox::critical( this, tr( "Aruni Gateway" ),
							   tr( "The gateway settings could not be saved. Please run the Configurator as administrator." ) );
	}

	refreshStatus();
}



QImage GatewayConfigurationPage::qrCode( const QString& text, int moduleSize )
{
	const auto code = qrcodegen::QrCode::encodeText( text.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM );
	const int border = 3;
	const int size = ( code.getSize() + 2 * border ) * moduleSize;

	QImage image( size, size, QImage::Format_RGB32 );
	image.fill( Qt::white );
	QPainter painter( &image );
	painter.setPen( Qt::NoPen );
	painter.setBrush( Qt::black );
	for( int y = 0; y < code.getSize(); ++y )
	{
		for( int x = 0; x < code.getSize(); ++x )
		{
			if( code.getModule( x, y ) )
			{
				painter.drawRect( ( x + border ) * moduleSize, ( y + border ) * moduleSize, moduleSize, moduleSize );
			}
		}
	}
	return image;
}



void GatewayConfigurationPage::startPairing()
{
	if( m_enabled->isChecked() == false )
	{
		m_enabled->setChecked( true );
	}
	applyConfiguration();

	const auto token = AruniTunnel::randomBytes( AruniTunnel::TokenSize );
	const auto expires = QDateTime::currentDateTimeUtc().addSecs( GatewayState::PairingValiditySeconds );
	if( GatewayState::update( [&]( GatewayState& state ) {
			state.pairingToken = token;
			state.pairingExpires = expires;
		} ) == false )
	{
		QMessageBox::critical( this, tr( "Aruni Gateway" ),
							   tr( "The pairing code could not be saved. Please run the Configurator as administrator." ) );
		return;
	}

	const auto state = GatewayState::load();
	const auto code = state.pairingInfo().encode();

	m_qrCode->setPixmap( QPixmap::fromImage( qrCode( code, 5 ) ) );
	m_pairingCode->setText( code );
	m_pairingHint->setText( tr( "Scan this code with the AruniControl app (location \"%1\"). "
								"Valid until %2." ).arg( state.siteName,
														 expires.toLocalTime().toString( QStringLiteral("HH:mm") ) ) );
	m_pairingBox->show();
}



void GatewayConfigurationPage::refreshStatus()
{
	const auto status = GatewayState::readStatus();
	const auto state = GatewayState::load();

	QString text;
	QString color = QStringLiteral("#888888");
	if( state.enabled == false )
	{
		text = tr( "Off" );
	}
	else if( status.value( QStringLiteral("running") ).toBool() == false )
	{
		text = tr( "Enabled - waiting for the AruniControl service" );
		color = QStringLiteral("#d48e00");
	}
	else if( status.value( QStringLiteral("connected") ).toBool() )
	{
		const auto sessions = status.value( QStringLiteral("sessions") ).toInt();
		text = sessions > 0 ? tr( "Online - %1 phone(s) connected" ).arg( sessions ) : tr( "Online" );
		color = QStringLiteral("#1f9d55");
	}
	else
	{
		text = tr( "Connecting to the relay server…" );
		const auto error = status.value( QStringLiteral("error") ).toString();
		if( error.isEmpty() == false )
		{
			text += QStringLiteral(" (%1)").arg( error.toHtmlEscaped() );
		}
		color = QStringLiteral("#dc3b3f");
	}

	m_status->setText( QStringLiteral("<span style='color:%1'>●</span> %2").arg( color, text ) );

	if( m_pairingBox->isVisible() && state.isPairingActive() == false )
	{
		// used (or expired) - the phone is listed below if pairing succeeded
		m_pairingBox->hide();
	}
}



void GatewayConfigurationPage::refreshDevices()
{
	const auto state = GatewayState::load();

	const auto selected = m_devices->currentRow();
	m_devices->setRowCount( int( state.devices.size() ) );
	int row = 0;
	for( const auto& device : state.devices )
	{
		auto nameItem = new QTableWidgetItem( device.name );
		nameItem->setData( Qt::UserRole, AruniTunnel::toBase64Url( device.publicKey ) );
		m_devices->setItem( row, 0, nameItem );
		m_devices->setItem( row, 1, new QTableWidgetItem( QLocale().toString( device.added.toLocalTime(), QLocale::ShortFormat ) ) );
		m_devices->setItem( row, 2, new QTableWidgetItem( QLocale().toString( device.lastSeen.toLocalTime(), QLocale::ShortFormat ) ) );
		++row;
	}
	m_devices->setCurrentCell( qMin( selected, m_devices->rowCount() - 1 ), 0 );
	m_removeButton->setEnabled( m_devices->rowCount() > 0 );
}



void GatewayConfigurationPage::removeSelectedDevice()
{
	const auto item = m_devices->item( m_devices->currentRow(), 0 );
	if( item == nullptr )
	{
		return;
	}

	if( QMessageBox::question( this, tr( "Remove access" ),
							   tr( "Remove access for \"%1\"? The phone has to be paired again to connect." ).arg( item->text() ) )
		!= QMessageBox::Yes )
	{
		return;
	}

	const auto key = AruniTunnel::fromBase64Url( item->data( Qt::UserRole ).toString() );
	GatewayState::update( [&key]( GatewayState& state ) {
		state.devices.removeIf( [&key]( const GatewayState::Device& device ) { return device.publicKey == key; } );
	} );
	refreshDevices();
}
