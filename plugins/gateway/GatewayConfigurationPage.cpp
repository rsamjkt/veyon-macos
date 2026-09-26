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
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QSpinBox>
#include <QTabWidget>
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

#include "ActivityLog.h"
#include "GatewayConfigurationPage.h"
#include "Notifier.h"
#include "MonitoringCollector.h"
#include "TimelapseView.h"
#include "Filesystem.h"
#include "GatewayState.h"
#include "RoamingAgent.h"
#include "VeyonConfiguration.h"
#include "VeyonCore.h"


GatewayConfigurationPage::GatewayConfigurationPage( QWidget* parent ) :
	ConfigurationPage( parent )
{
	setWindowTitle( tr( "Aruni Gateway" ) );
	m_notifier = new Notifier( this );
	setWindowIcon( QIcon( QStringLiteral(":/core/application-x-pem-key.png") ) );

	auto pageLayout = new QVBoxLayout( this );
	pageLayout->setContentsMargins( 0, 0, 0, 0 );
	m_tabs = new QTabWidget;
	pageLayout->addWidget( m_tabs );

	auto gatewayTab = new QWidget;
	auto layout = new QVBoxLayout( gatewayTab );
	m_tabs->addTab( gatewayTab, tr( "Gateway" ) );

	auto laptopsTab = new QWidget;
	auto laptopsTabLayout = new QVBoxLayout( laptopsTab );
	m_laptopsTabIndex = m_tabs->addTab( laptopsTab, tr( "Laptops" ) );

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
	laptopsTabLayout->addWidget( laptopsBox, 1 );
	laptopsTabLayout->addWidget( createScreenshotBox() );

	m_notificationsTabIndex = m_tabs->addTab( createNotificationsTab(), tr( "Notifications" ) );
	m_activityTabIndex = m_tabs->addTab( createActivityTab(), tr( "Activity" ) );
	m_timelapse = new TimelapseView;
	m_recordingsTabIndex = m_tabs->addTab( m_timelapse, tr( "Recordings" ) );

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
		if( m_tabs->currentIndex() == m_activityTabIndex )
		{
			refreshActivity();
		}
	} );
	connect( m_tabs, &QTabWidget::currentChanged, this, [this]( int index ) {
		if( index == m_activityTabIndex )
		{
			refreshActivity();
		}
		else if( index == m_recordingsTabIndex )
		{
			m_timelapse->refresh();
		}
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

	m_screenshotInterval->setCurrentIndex( qMax( 0, m_screenshotInterval->findData( state.screenshotInterval ) ) );
	m_screenshotRetention->setValue( state.screenshotRetentionDays );
	m_screenshotInOffice->setChecked( state.screenshotInOffice );
	m_screenshotAllComputers->setChecked( state.screenshotAllComputers );
	m_collectAccessLogs->setChecked( state.collectAccessLogs );
	m_telegramToken->setText( state.telegramToken );
	m_telegramChatId->setText( state.telegramChatId );
	m_offlineAlertHours->setValue( state.offlineAlertHours );
	m_notifyRefused->setChecked( state.notifyRefused );
	m_notifyNewLaptop->setChecked( state.notifyNewLaptop );

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
	connect( m_screenshotInterval, &QComboBox::currentIndexChanged, this, &ConfigurationPage::widgetsChanged );
	connect( m_screenshotRetention, &QSpinBox::valueChanged, this, &ConfigurationPage::widgetsChanged );
	connect( m_screenshotInOffice, &QCheckBox::toggled, this, &ConfigurationPage::widgetsChanged );
	connect( m_screenshotAllComputers, &QCheckBox::toggled, this, &ConfigurationPage::widgetsChanged );
	connect( m_collectAccessLogs, &QCheckBox::toggled, this, &ConfigurationPage::widgetsChanged );
	connect( m_telegramToken, &QLineEdit::textChanged, this, &ConfigurationPage::widgetsChanged );
	connect( m_telegramChatId, &QLineEdit::textChanged, this, &ConfigurationPage::widgetsChanged );
	connect( m_offlineAlertHours, &QSpinBox::valueChanged, this, &ConfigurationPage::widgetsChanged );
	connect( m_notifyRefused, &QCheckBox::toggled, this, &ConfigurationPage::widgetsChanged );
	connect( m_notifyNewLaptop, &QCheckBox::toggled, this, &ConfigurationPage::widgetsChanged );
}



void GatewayConfigurationPage::applyConfiguration()
{
	const auto enabled = m_enabled->isChecked();
	const auto siteName = m_siteName->text().trimmed();
	const auto relayUrl = m_relayUrl->text().trimmed();
	const auto sharedKey = m_sharedKey->currentData().toString();
	const auto screenshotInterval = m_screenshotInterval->currentData().toInt();
	const auto screenshotRetention = m_screenshotRetention->value();
	const auto screenshotInOffice = m_screenshotInOffice->isChecked();
	const auto screenshotAllComputers = m_screenshotAllComputers->isChecked();
	const auto collectAccessLogs = m_collectAccessLogs->isChecked();
	const auto telegramToken = m_telegramToken->text().trimmed();
	const auto telegramChatId = m_telegramChatId->text().trimmed();
	const auto offlineAlertHours = m_offlineAlertHours->value();
	const auto notifyRefused = m_notifyRefused->isChecked();
	const auto notifyNewLaptop = m_notifyNewLaptop->isChecked();

	if( GatewayState::update( [=]( GatewayState& state ) {
			state.screenshotInterval = screenshotInterval;
			state.screenshotRetentionDays = screenshotRetention;
			state.screenshotInOffice = screenshotInOffice;
			state.screenshotAllComputers = screenshotAllComputers;
			state.collectAccessLogs = collectAccessLogs;
			state.telegramToken = telegramToken;
			state.telegramChatId = telegramChatId;
			state.offlineAlertHours = offlineAlertHours;
			state.notifyRefused = notifyRefused;
			state.notifyNewLaptop = notifyNewLaptop;
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
	m_tabs->setTabVisible( m_laptopsTabIndex, state.enabled );
	m_tabs->setTabVisible( m_notificationsTabIndex, state.enabled );
	m_tabs->setTabVisible( m_activityTabIndex, state.enabled );
	m_tabs->setTabVisible( m_recordingsTabIndex, state.enabled );
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
			m_laptops->setItem( row, 2, new QTableWidgetItem( QString::number( GatewayState::agentPort( agent.slot ) ) ) );
			m_laptops->setItem( row, 3, new QTableWidgetItem( online.contains( key ) ? tr( "now" ) :
				QLocale().toString( agent.lastSeen.toLocalTime(), QLocale::ShortFormat ) ) );
			++row;
		}
		m_laptops->setCurrentCell( qMin( selected, m_laptops->rowCount() - 1 ), 0 );
		m_removeLaptopButton->setEnabled( m_laptops->rowCount() > 0 );
	}

	if( state.enabled )
	{
		const auto keyName = MonitoringCollector::availableKeyName( state.sharedKeyName );
		const auto error = GatewayState::readStatus()[QStringLiteral("screenshotError")].toString();
		if( m_screenshotInterval->currentData().toInt() <= 0 )
		{
			m_screenshotStatus->setText( tr( "Off" ) );
		}
		else if( keyName.isEmpty() )
		{
			m_screenshotStatus->setText( QStringLiteral("<span style='color:#dc3b3f'>●</span> %1").arg(
				tr( "This computer needs a private authentication key (Authentication keys) to connect to the laptops" ).toHtmlEscaped() ) );
		}
		else if( error.isEmpty() == false )
		{
			m_screenshotStatus->setText( QStringLiteral("<span style='color:#dc3b3f'>●</span> %1").arg( error.toHtmlEscaped() ) );
		}
		else
		{
			m_screenshotStatus->setText( QStringLiteral("<span style='color:#1f9d55'>●</span> %1").arg(
				tr( "Active, using the key \"%1\"" ).arg( keyName ).toHtmlEscaped() ) );
		}
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
		const auto blocked = RoamingAgent::forwardingBlockedReason();
		if( blocked.isEmpty() == false )
		{
			text = blocked.toHtmlEscaped();
			color = QStringLiteral("#dc3b3f");
		}
		else if( status.value( QStringLiteral("running") ).toBool() == false )
		{
			text = tr( "Enabled - waiting for the AruniControl service" );
			color = QStringLiteral("#d48e00");
		}
		else if( status.value( QStringLiteral("connected") ).toBool() )
		{
			text = status.value( QStringLiteral("inOffice") ).toBool() ?
					   tr( "Connected to \"%1\" - currently in the office" ).arg( site ) :
					   tr( "Connected to \"%1\" - this laptop can be monitored from anywhere" ).arg( site );
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



QWidget* GatewayConfigurationPage::createScreenshotBox()
{
	auto box = new QGroupBox( tr( "Scheduled screenshots" ) );
	auto layout = new QVBoxLayout( box );

	auto intro = new QLabel( tr( "Keeps screenshots of the computers at regular intervals as an audit trail - watch them "
								 "as a timelapse in the Recordings tab. This computer connects to them like a Master, so "
								 "it needs a private authentication key whose public key is installed on the computers." ) );
	intro->setWordWrap( true );
	layout->addWidget( intro );

	auto form = new QFormLayout;
	m_screenshotInterval = new QComboBox;
	m_screenshotInterval->addItem( tr( "Off" ), 0 );
	for( const auto minutes : { 5, 10, 15, 30, 60 } )
	{
		m_screenshotInterval->addItem( tr( "Every %1 minutes" ).arg( minutes ), minutes );
	}
	form->addRow( tr( "Take a screenshot" ), m_screenshotInterval );

	m_screenshotRetention = new QSpinBox;
	m_screenshotRetention->setRange( 1, 365 );
	m_screenshotRetention->setSuffix( tr( " days" ) );
	form->addRow( tr( "Keep screenshots for" ), m_screenshotRetention );

	m_screenshotAllComputers = new QCheckBox( tr( "All computers of the office, not only roaming laptops" ) );
	form->addRow( QString{}, m_screenshotAllComputers );
	m_screenshotInOffice = new QCheckBox( tr( "Roaming laptops also while they are in the office" ) );
	form->addRow( QString{}, m_screenshotInOffice );

	m_screenshotStatus = new QLabel;
	m_screenshotStatus->setTextFormat( Qt::RichText );
	m_screenshotStatus->setWordWrap( true );
	form->addRow( tr( "Status" ), m_screenshotStatus );
	layout->addLayout( form );

	auto openButton = new QPushButton( tr( "Open screenshots folder" ) );
	connect( openButton, &QPushButton::clicked, this, []() {
		QDir().mkpath( MonitoringCollector::screenshotDirectory() );
		QDesktopServices::openUrl( QUrl::fromLocalFile( MonitoringCollector::screenshotDirectory() ) );
	} );
	layout->addWidget( openButton, 0, Qt::AlignLeft );

	return box;
}



QWidget* GatewayConfigurationPage::createNotificationsTab()
{
	auto tab = new QWidget;
	auto layout = new QVBoxLayout( tab );

	auto telegramBox = new QGroupBox( tr( "Alerts to your phone (Telegram)" ) );
	auto telegramLayout = new QVBoxLayout( telegramBox );
	auto intro = new QLabel( tr( "1. In Telegram, open @BotFather, send /newbot and follow the steps. Copy the bot token.\n"
								 "2. Paste the token below, then open your new bot in Telegram and send /start.\n"
								 "3. Click \"Detect\" to fill in the chat ID, click \"Send test\" and then Apply.\n"
								 "Tip: add the bot to a group and send /start there to alert the whole IT team." ) );
	intro->setWordWrap( true );
	intro->setTextFormat( Qt::PlainText );
	telegramLayout->addWidget( intro );

	auto form = new QFormLayout;
	m_telegramToken = new QLineEdit;
	m_telegramToken->setPlaceholderText( QStringLiteral("123456789:AA…") );
	form->addRow( tr( "Bot token" ), m_telegramToken );

	auto chatRow = new QHBoxLayout;
	m_telegramChatId = new QLineEdit;
	chatRow->addWidget( m_telegramChatId, 1 );
	auto detectButton = new QPushButton( tr( "Detect" ) );
	chatRow->addWidget( detectButton );
	form->addRow( tr( "Chat ID" ), chatRow );
	telegramLayout->addLayout( form );

	auto testRow = new QHBoxLayout;
	auto testButton = new QPushButton( tr( "Send test" ) );
	testRow->addWidget( testButton );
	m_notificationStatus = new QLabel;
	m_notificationStatus->setWordWrap( true );
	testRow->addWidget( m_notificationStatus, 1 );
	telegramLayout->addLayout( testRow );
	layout->addWidget( telegramBox );

	auto eventsBox = new QGroupBox( tr( "Send an alert when" ) );
	auto eventsLayout = new QFormLayout( eventsBox );
	m_offlineAlertHours = new QSpinBox;
	m_offlineAlertHours->setRange( 0, 24 * 30 );
	m_offlineAlertHours->setSuffix( tr( " hours" ) );
	m_offlineAlertHours->setSpecialValueText( tr( "never" ) );
	eventsLayout->addRow( tr( "a roaming laptop is offline longer than" ), m_offlineAlertHours );
	m_notifyRefused = new QCheckBox( tr( "an access attempt is refused (unknown phone or laptop)" ) );
	eventsLayout->addRow( QString{}, m_notifyRefused );
	m_notifyNewLaptop = new QCheckBox( tr( "a new roaming laptop registers" ) );
	eventsLayout->addRow( QString{}, m_notifyNewLaptop );
	layout->addWidget( eventsBox );
	layout->addStretch( 1 );

	connect( detectButton, &QPushButton::clicked, this, &GatewayConfigurationPage::detectTelegramChat );
	connect( testButton, &QPushButton::clicked, this, &GatewayConfigurationPage::sendTestNotification );

	return tab;
}



QWidget* GatewayConfigurationPage::createActivityTab()
{
	auto tab = new QWidget;
	auto layout = new QVBoxLayout( tab );

	m_collectAccessLogs = new QCheckBox( tr( "Collect the access logs of all computers (every 15 minutes): who connected "
											 "to which computer, when and with which functions" ) );
	layout->addWidget( m_collectAccessLogs );

	auto row = new QHBoxLayout;
	row->addWidget( new QLabel( tr( "Show" ) ) );
	m_activityRange = new QComboBox;
	m_activityRange->addItem( tr( "Last 24 hours" ), 1 );
	m_activityRange->addItem( tr( "Last 7 days" ), 7 );
	m_activityRange->addItem( tr( "Last 30 days" ), 30 );
	m_activityRange->addItem( tr( "Everything" ), 0 );
	m_activityRange->setCurrentIndex( 1 );
	row->addWidget( m_activityRange );
	row->addStretch( 1 );
	auto exportButton = new QPushButton( tr( "Export to Excel (CSV)…" ) );
	row->addWidget( exportButton );
	layout->addLayout( row );

	m_activity = new QTableWidget( 0, 4 );
	m_activity->setHorizontalHeaderLabels( { tr( "Time" ), tr( "Event" ), tr( "Laptop / phone" ), tr( "Details" ) } );
	m_activity->horizontalHeader()->setSectionResizeMode( 0, QHeaderView::ResizeToContents );
	m_activity->horizontalHeader()->setSectionResizeMode( 1, QHeaderView::ResizeToContents );
	m_activity->horizontalHeader()->setSectionResizeMode( 2, QHeaderView::Interactive );
	m_activity->horizontalHeader()->resizeSection( 2, 160 );
	m_activity->horizontalHeader()->setSectionResizeMode( 3, QHeaderView::Stretch );
	m_activity->setWordWrap( false );
	m_activity->setTextElideMode( Qt::ElideMiddle );
	m_activity->verticalHeader()->hide();
	m_activity->setSelectionBehavior( QAbstractItemView::SelectRows );
	m_activity->setEditTriggers( QAbstractItemView::NoEditTriggers );
	m_activity->setMinimumHeight( 300 );
	layout->addWidget( m_activity, 1 );

	connect( m_activityRange, &QComboBox::currentIndexChanged, this, [this]() {
		m_activityModified = {};
		refreshActivity();
	} );
	connect( exportButton, &QPushButton::clicked, this, &GatewayConfigurationPage::exportActivity );

	return tab;
}



void GatewayConfigurationPage::refreshActivity()
{
	const auto modified = QFileInfo( ActivityLog::logPath() ).lastModified();
	if( modified == m_activityModified && m_activity->rowCount() > 0 )
	{
		return;
	}
	m_activityModified = modified;

	const auto days = m_activityRange->currentData().toInt();
	const auto since = days > 0 ? QDateTime::currentDateTimeUtc().addDays( -days ) : QDateTime{};
	const auto entries = ActivityLog::read( 2000, since );

	m_activity->setRowCount( int( entries.size() ) );
	int row = 0;
	for( const auto& entry : entries )
	{
		m_activity->setItem( row, 0, new QTableWidgetItem( QLocale().toString( entry.time.toLocalTime(), QLocale::ShortFormat ) ) );
		m_activity->setItem( row, 1, new QTableWidgetItem( ActivityLog::eventName( entry.event ) ) );
		m_activity->setItem( row, 2, new QTableWidgetItem( entry.subject ) );
		auto details = new QTableWidgetItem( ActivityLog::describe( entry ) );
		details->setToolTip( details->text() );
		m_activity->setItem( row, 3, details );
		++row;
	}
}



void GatewayConfigurationPage::exportActivity()
{
	const auto fileName = QFileDialog::getSaveFileName( this, tr( "Export activity" ),
		QDir::home().filePath( QStringLiteral("aruni-activity-%1.csv").arg( QDate::currentDate().toString( Qt::ISODate ) ) ),
		tr( "CSV files (*.csv)" ) );
	if( fileName.isEmpty() )
	{
		return;
	}

	const auto days = m_activityRange->currentData().toInt();
	if( ActivityLog::exportCsv( fileName, days > 0 ? QDateTime::currentDateTimeUtc().addDays( -days ) : QDateTime{} ) == false )
	{
		QMessageBox::critical( this, tr( "Export activity" ), tr( "Could not write %1." ).arg( fileName ) );
	}
}



void GatewayConfigurationPage::detectTelegramChat()
{
	m_notificationStatus->setText( tr( "Detecting…" ) );
	m_notifier->detectChatId( m_telegramToken->text().trimmed(), [this]( const QString& chatId, const QString& error ) {
		if( chatId.isEmpty() )
		{
			m_notificationStatus->setText( error );
			return;
		}
		m_telegramChatId->setText( chatId );
		m_notificationStatus->setText( tr( "Chat found. Click \"Send test\"." ) );
	} );
}



void GatewayConfigurationPage::sendTestNotification()
{
	m_notificationStatus->setText( tr( "Sending…" ) );
	m_notifier->send( m_telegramToken->text().trimmed(), m_telegramChatId->text().trimmed(),
					  tr( "✅ AruniControl alerts work. Location: %1" ).arg( m_siteName->text() ),
					  [this]( bool ok, const QString& error ) {
		m_notificationStatus->setText( ok ? tr( "Sent - check Telegram, then click Apply." ) : error );
	} );
}
