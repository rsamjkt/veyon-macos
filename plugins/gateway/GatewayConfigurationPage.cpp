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
#include <QDir>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
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
	m_pairBox = pairBox;
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
	m_devicesBox = devicesBox;
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

	// --- roaming laptops (office gateway side)
	auto laptopsBox = new QGroupBox( tr( "Laptops outside the office" ) );
	m_laptopsBox = laptopsBox;
	auto laptopsLayout = new QVBoxLayout( laptopsBox );
	auto laptopsIntro = new QLabel( tr( "Laptops that are taken home stay monitored: enter this enrollment code on the laptop "
										"(AruniControl Configurator → Aruni Gateway → \"This laptop outside the office\", or "
										"\"veyon-cli gateway enroll <code>\" for many laptops at once). Outside the office "
										"they appear in the app as \"Outside the office\" and in the Master on this network as "
										"\"<this computer>:<port>\"." ) );
	laptopsIntro->setWordWrap( true );
	laptopsIntro->setTextFormat( Qt::PlainText );
	laptopsLayout->addWidget( laptopsIntro );
	auto codeRow = new QHBoxLayout;
	m_enrollmentCode = new QLineEdit;
	m_enrollmentCode->setReadOnly( true );
	codeRow->addWidget( m_enrollmentCode, 1 );
	auto copyCodeButton = new QPushButton( tr( "Copy" ) );
	codeRow->addWidget( copyCodeButton );
	auto renewCodeButton = new QPushButton( tr( "New code" ) );
	renewCodeButton->setToolTip( tr( "Laptops that are already registered keep working" ) );
	codeRow->addWidget( renewCodeButton );
	laptopsLayout->addLayout( codeRow );
	m_laptops = new QTableWidget( 0, 4 );
	m_laptops->setHorizontalHeaderLabels( { tr( "Laptop" ), tr( "Status" ), tr( "Port" ), tr( "Last seen" ) } );
	m_laptops->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
	m_laptops->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::Stretch );
	m_laptops->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::ResizeToContents );
	m_laptops->horizontalHeader()->setSectionResizeMode( 3, QHeaderView::ResizeToContents );
	m_laptops->verticalHeader()->hide();
	m_laptops->setSelectionBehavior( QAbstractItemView::SelectRows );
	m_laptops->setEditTriggers( QAbstractItemView::NoEditTriggers );
	m_laptops->setMinimumHeight( 120 );
	laptopsLayout->addWidget( m_laptops );
	m_removeLaptopButton = new QPushButton( tr( "Remove laptop" ) );
	laptopsLayout->addWidget( m_removeLaptopButton, 0, Qt::AlignLeft );
	layout->addWidget( laptopsBox, 1 );

	// --- this computer as roaming laptop
	auto roamingBox = new QGroupBox( tr( "This laptop outside the office" ) );
	m_roamingBox = roamingBox;
	auto roamingLayout = new QVBoxLayout( roamingBox );
	auto roamingIntro = new QLabel( tr( "Keeps this laptop reachable for the Master and the app of the office when it is used "
										"elsewhere (at home, on mobile data). It connects to the Aruni Gateway of the office; "
										"only this computer can be reached this way, never other devices of the network it is in." ) );
	roamingIntro->setWordWrap( true );
	roamingLayout->addWidget( roamingIntro );
	m_roamingEnabled = new QCheckBox( tr( "Keep this laptop monitored outside the office" ) );
	roamingLayout->addWidget( m_roamingEnabled );
	auto roamingForm = new QFormLayout;
	m_roamingCode = new QLineEdit;
	m_roamingCode->setPlaceholderText( tr( "Enrollment code from the office gateway (ARUNIL1:…)" ) );
	roamingForm->addRow( tr( "Enrollment code" ), m_roamingCode );
	m_roamingStatus = new QLabel;
	m_roamingStatus->setTextFormat( Qt::RichText );
	m_roamingStatus->setWordWrap( true );
	roamingForm->addRow( tr( "Status" ), m_roamingStatus );
	roamingLayout->addLayout( roamingForm );
	// right below the general settings - on a laptop it is the only relevant part
	layout->insertWidget( 1, roamingBox );
	// keeps the boxes compact when the lists are hidden (on a laptop)
	layout->addStretch( 1 );

	connect( copyCodeButton, &QPushButton::clicked, this, [this]() {
		QApplication::clipboard()->setText( m_enrollmentCode->text() );
	} );
	connect( renewCodeButton, &QPushButton::clicked, this, &GatewayConfigurationPage::renewEnrollmentCode );
	connect( m_removeLaptopButton, &QPushButton::clicked, this, &GatewayConfigurationPage::removeSelectedLaptop );
	connect( m_roamingEnabled, &QCheckBox::toggled, this, &GatewayConfigurationPage::applyRoaming );
	connect( m_roamingCode, &QLineEdit::editingFinished, this, [this]() {
		if( m_roamingEnabled->isChecked() )
		{
			applyRoaming();
		}
	} );

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
		refreshRoaming();
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

	const QSignalBlocker roamingBlocker( m_roamingEnabled );
	m_roamingEnabled->setChecked( state.roamingEnabled );
	m_roamingCode->setText( state.roamingHub.gatewayId.isEmpty() ? QString{} : state.roamingHub.encode() );

	refreshStatus();
	refreshDevices();
	refreshRoaming();
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
	refreshRoaming();
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



void GatewayConfigurationPage::refreshRoaming()
{
	const auto state = GatewayState::load();

	// a computer is either the office gateway or a roaming laptop
	m_laptopsBox->setVisible( state.enabled );
	m_pairBox->setVisible( state.enabled );
	m_devicesBox->setVisible( state.enabled );
	m_roamingBox->setVisible( state.enabled == false );
	m_enabled->setEnabled( state.roamingEnabled == false );

	if( state.enabled )
	{
		const auto code = state.enrollmentInfo().encode();
		if( m_enrollmentCode->text() != code )
		{
			m_enrollmentCode->setText( code );
		}

		QHash<QString, QJsonObject> online;
		for( const auto& value : GatewayState::readStatus()[QStringLiteral("agents")].toArray() )
		{
			const auto object = value.toObject();
			online.insert( object[QStringLiteral("key")].toString(), object );
		}

		const auto selected = m_laptops->currentRow();
		m_laptops->setRowCount( int( state.agents.size() ) );
		int row = 0;
		for( const auto& agent : state.agents )
		{
			const auto key = AruniTunnel::toBase64Url( agent.publicKey );
			auto nameItem = new QTableWidgetItem( agent.name );
			nameItem->setData( Qt::UserRole, key );
			m_laptops->setItem( row, 0, nameItem );

			auto statusItem = new QTableWidgetItem( tr( "Offline" ) );
			if( online.contains( key ) )
			{
				statusItem->setText( online[key][QStringLiteral("local")].toBool() ? tr( "In the office" ) : tr( "Online, away" ) );
				const auto user = online[key][QStringLiteral("user")].toString();
				if( user.isEmpty() == false )
				{
					statusItem->setToolTip( tr( "Logged on: %1" ).arg( user ) );
				}
			}
			m_laptops->setItem( row, 1, statusItem );
			m_laptops->setItem( row, 2, new QTableWidgetItem( QString::number( VeyonCore::config().veyonServerPort() + agent.slot ) ) );
			m_laptops->setItem( row, 3, new QTableWidgetItem( online.contains( key ) ? tr( "now" ) :
				QLocale().toString( agent.lastSeen.toLocalTime(), QLocale::ShortFormat ) ) );
			++row;
		}
		m_laptops->setCurrentCell( qMin( selected, m_laptops->rowCount() - 1 ), 0 );
		m_removeLaptopButton->setEnabled( m_laptops->rowCount() > 0 );
	}

	QString text;
	QString color = QStringLiteral("#888888");
	if( state.enabled )
	{
		text = tr( "Not available on the Aruni Gateway itself" );
	}
	else if( state.roamingEnabled == false )
	{
		text = tr( "Off" );
	}
	else
	{
		const auto status = GatewayState::readStatus( QStringLiteral("roaming-status.json") );
		const auto site = state.roamingHub.siteName.toHtmlEscaped();
		if( status.value( QStringLiteral("running") ).toBool() == false )
		{
			text = tr( "Enabled - waiting for the AruniControl service" );
			color = QStringLiteral("#d48e00");
		}
		else if( status.value( QStringLiteral("connected") ).toBool() )
		{
			text = tr( "Connected to \"%1\" - this laptop can be monitored from anywhere" ).arg( site );
			color = QStringLiteral("#1f9d55");
		}
		else
		{
			text = tr( "Connecting to \"%1\"…" ).arg( site );
			const auto error = status.value( QStringLiteral("error") ).toString();
			if( error.isEmpty() == false )
			{
				text += QStringLiteral(" (%1)").arg( error.toHtmlEscaped() );
			}
			color = QStringLiteral("#dc3b3f");
		}
	}
	m_roamingStatus->setText( QStringLiteral("<span style='color:%1'>●</span> %2").arg( color, text ) );
}



void GatewayConfigurationPage::renewEnrollmentCode()
{
	if( QMessageBox::question( this, tr( "New enrollment code" ),
							   tr( "Create a new enrollment code? The current code can no longer be used to add laptops. "
								   "Laptops that are already registered keep working." ) ) != QMessageBox::Yes )
	{
		return;
	}

	GatewayState::update( []( GatewayState& state ) {
		state.enrollmentToken = AruniTunnel::randomBytes( AruniTunnel::TokenSize );
	} );
	refreshRoaming();
}



void GatewayConfigurationPage::removeSelectedLaptop()
{
	const auto item = m_laptops->item( m_laptops->currentRow(), 0 );
	if( item == nullptr )
	{
		return;
	}

	if( QMessageBox::question( this, tr( "Remove laptop" ),
							   tr( "Remove \"%1\"? It can no longer be reached outside the office until it is enrolled "
								   "again with a new enrollment code." ).arg( item->text() ) ) != QMessageBox::Yes )
	{
		return;
	}

	// a removed laptop must not be able to re-enroll with the code it knows
	const auto key = AruniTunnel::fromBase64Url( item->data( Qt::UserRole ).toString() );
	GatewayState::update( [&key]( GatewayState& state ) {
		state.agents.removeIf( [&key]( const GatewayState::Agent& agent ) { return agent.publicKey == key; } );
		state.enrollmentToken = AruniTunnel::randomBytes( AruniTunnel::TokenSize );
	} );
	refreshRoaming();
}



void GatewayConfigurationPage::applyRoaming()
{
	const bool enabled = m_roamingEnabled->isChecked();
	const auto code = m_roamingCode->text().trimmed();
	const auto hub = AruniTunnel::PairingInfo::decode( code );

	if( enabled && ( hub.enrollment == false || hub.isValid() == false ) )
	{
		QMessageBox::warning( this, tr( "Aruni Gateway" ),
							  tr( "Please enter the enrollment code shown by the Aruni Gateway of the office "
								  "(section \"Laptops outside the office\"). It starts with ARUNIL1:" ) );
		const QSignalBlocker blocker( m_roamingEnabled );
		m_roamingEnabled->setChecked( false );
		return;
	}

	if( GatewayState::update( [=]( GatewayState& state ) {
			const bool hubChanged = state.roamingHub.gatewayId != hub.gatewayId || state.roamingHub.token != hub.token;
			state.roamingEnabled = enabled;
			if( enabled && hubChanged )
			{
				state.roamingHub = hub;
				state.roamingRegistered = false;
			}
		} ) == false )
	{
		QMessageBox::critical( this, tr( "Aruni Gateway" ),
							   tr( "The settings could not be saved. Please run the Configurator as administrator." ) );
	}

	refreshRoaming();
}
