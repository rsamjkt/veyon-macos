/*
 * DeviceControlFeaturePlugin.cpp - block USB storage and printing
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
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QProcess>
#include <QSettings>
#include <QVBoxLayout>

#include "DeviceControlFeaturePlugin.h"
#include "VeyonCore.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"


namespace {

#if defined(Q_OS_WIN)
const auto UsbStorKey = QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\USBSTOR");
const auto SpoolerKey = QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\Spooler");
const auto RemovableStorageKey = QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\Windows\\RemovableStorageDevices");
constexpr int ServiceDisabled = 4;
constexpr int ServiceDemandStart = 3;

bool runSc( const QStringList& arguments )
{
	return QProcess::execute( QStringLiteral("sc.exe"), arguments ) == 0;
}
#endif

}



DeviceControlFeaturePlugin::DeviceControlFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_feature( Feature( QStringLiteral( "DeviceControl" ),
						Feature::Flag::Action | Feature::Flag::AllComponents,
						Feature::Uid( FeatureUid ),
						Feature::Uid(),
						tr( "USB & printer" ), {},
						tr( "Block or allow USB flash drives and printing on the selected computers." ),
						QStringLiteral(":/devicecontrol/devicecontrol.png") ) ),
	m_features( { m_feature } )
{
}



const FeatureList& DeviceControlFeaturePlugin::featureList() const
{
	return m_features;
}



bool DeviceControlFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
												 const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( featureUid != m_feature.uid() )
	{
		return false;
	}

	if( operation == Operation::Initialize )
	{
		sendFeatureMessage( FeatureMessage{ featureUid, Query }, computerControlInterfaces );
		return true;
	}

	if( operation != Operation::Start )
	{
		return false;
	}

	sendFeatureMessage( FeatureMessage{ featureUid, Set }
							.addArgument( Argument::Usb, arguments.value( QStringLiteral("usb") ).toString() )
							.addArgument( Argument::Printer, arguments.value( QStringLiteral("printer") ).toString() ),
						computerControlInterfaces );
	return true;
}



bool DeviceControlFeaturePlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
											   const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( feature.uid() != m_feature.uid() || computerControlInterfaces.isEmpty() )
	{
		return false;
	}

	QDialog dialog( master.mainWindow() );
	dialog.setWindowTitle( tr( "USB & printer" ) );
	auto layout = new QVBoxLayout( &dialog );
	layout->addWidget( new QLabel( tr( "%n selected computer(s) (Windows):", nullptr, int( computerControlInterfaces.size() ) ) ) );
	auto usb = new QCheckBox( tr( "Block USB flash drives and external disks" ) );
	auto printer = new QCheckBox( tr( "Block printing" ) );
	layout->addWidget( usb );
	layout->addWidget( printer );
	auto status = new QLabel( tr( "Checking the current state..." ) );
	status->setWordWrap( true );
	status->setStyleSheet( QStringLiteral("color: gray;") );
	layout->addWidget( status );
	auto hint = new QLabel( tr( "Flash drives that are already plugged in stay usable until they are unplugged. "
								"Keyboards, mice and other USB devices keep working." ) );
	hint->setWordWrap( true );
	layout->addWidget( hint );
	auto buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel );
	connect( buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept );
	connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
	layout->addWidget( buttons );

	// the checkboxes start with the state of the computers
	int answers = 0;
	int usbBlocked = 0;
	int printerBlocked = 0;
	int unsupported = 0;
	bool edited = false;
	connect( usb, &QCheckBox::clicked, &dialog, [&edited]() { edited = true; } );
	connect( printer, &QCheckBox::clicked, &dialog, [&edited]() { edited = true; } );
	const auto connection = connect( this, &DeviceControlFeaturePlugin::statusReceived, &dialog,
		[&]( ComputerControlInterface::Pointer, bool usbState, bool printerState, bool supported, const QString& ) {
			++answers;
			usbBlocked += usbState ? 1 : 0;
			printerBlocked += printerState ? 1 : 0;
			unsupported += supported ? 0 : 1;
			if( edited == false )
			{
				usb->setChecked( usbBlocked > 0 && usbBlocked == answers - unsupported );
				printer->setChecked( printerBlocked > 0 && printerBlocked == answers - unsupported );
			}
			status->setText( tr( "%1 of %2 computers answered: USB blocked on %3, printing blocked on %4%5" )
								 .arg( answers ).arg( computerControlInterfaces.size() ).arg( usbBlocked ).arg( printerBlocked )
								 .arg( unsupported > 0 ? tr( ", %1 not supported (not Windows)" ).arg( unsupported ) : QString{} ) );
		} );
	controlFeature( m_feature.uid(), Operation::Initialize, {}, computerControlInterfaces );

	if( dialog.exec() == QDialog::Accepted )
	{
		controlFeature( m_feature.uid(), Operation::Start, {
			{ QStringLiteral("usb"), usb->isChecked() ? QStringLiteral("block") : QStringLiteral("allow") },
			{ QStringLiteral("printer"), printer->isChecked() ? QStringLiteral("block") : QStringLiteral("allow") },
		}, computerControlInterfaces );
	}
	disconnect( connection );
	return true;
}



bool DeviceControlFeaturePlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
													   const FeatureMessage& message )
{
	if( message.featureUid() != m_feature.uid() )
	{
		return false;
	}
	if( static_cast<int>( message.command() ) == Status )
	{
		Q_EMIT statusReceived( computerControlInterface, message.argument( Argument::UsbBlocked ).toBool(),
							   message.argument( Argument::PrinterBlocked ).toBool(),
							   message.argument( Argument::Supported ).toBool(), message.argument( Argument::Error ).toString() );
	}
	return true;
}



bool DeviceControlFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
													   const FeatureMessage& message )
{
	if( message.featureUid() != m_feature.uid() )
	{
		return false;
	}

	QString error;
#if defined(Q_OS_WIN)
	const bool supported = true;
	if( static_cast<int>( message.command() ) == Set )
	{
		const auto usb = message.argument( Argument::Usb ).toString();
		const auto printer = message.argument( Argument::Printer ).toString();
		if( usb.isEmpty() == false && setUsbBlocked( usb == QStringLiteral("block") ) == false )
		{
			error = tr( "Could not change the USB setting" );
		}
		if( printer.isEmpty() == false && setPrinterBlocked( printer == QStringLiteral("block") ) == false )
		{
			error = tr( "Could not change the print spooler" );
		}
		vInfo() << "device control: usb" << usb << "printer" << printer << error;
	}
#else
	const bool supported = false;
	error = tr( "Only supported on Windows" );
#endif

	return server.sendFeatureMessageReply( messageContext, FeatureMessage{ m_feature.uid(), Status }
		.addArgument( Argument::UsbBlocked, isUsbBlocked() )
		.addArgument( Argument::PrinterBlocked, isPrinterBlocked() )
		.addArgument( Argument::Supported, supported )
		.addArgument( Argument::Error, error ) );
}



bool DeviceControlFeaturePlugin::isUsbBlocked()
{
#if defined(Q_OS_WIN)
	const QSettings usbStor( UsbStorKey, QSettings::NativeFormat );
	return usbStor.value( QStringLiteral("Start") ).toInt() == ServiceDisabled;
#else
	return false;
#endif
}



bool DeviceControlFeaturePlugin::isPrinterBlocked()
{
#if defined(Q_OS_WIN)
	const QSettings spooler( SpoolerKey, QSettings::NativeFormat );
	return spooler.value( QStringLiteral("Start") ).toInt() == ServiceDisabled;
#else
	return false;
#endif
}



bool DeviceControlFeaturePlugin::setUsbBlocked( bool blocked )
{
#if defined(Q_OS_WIN)
	QSettings usbStor( UsbStorKey, QSettings::NativeFormat );
	usbStor.setValue( QStringLiteral("Start"), blocked ? ServiceDisabled : ServiceDemandStart );
	QSettings policy( RemovableStorageKey, QSettings::NativeFormat );
	if( blocked )
	{
		policy.setValue( QStringLiteral("Deny_All"), 1 );
	}
	else
	{
		policy.remove( QStringLiteral("Deny_All") );
	}
	usbStor.sync();
	policy.sync();
	return usbStor.status() == QSettings::NoError && policy.status() == QSettings::NoError;
#else
	Q_UNUSED(blocked)
	return false;
#endif
}



bool DeviceControlFeaturePlugin::setPrinterBlocked( bool blocked )
{
#if defined(Q_OS_WIN)
	if( blocked )
	{
		const bool configured = runSc( { QStringLiteral("config"), QStringLiteral("Spooler"), QStringLiteral("start="), QStringLiteral("disabled") } );
		runSc( { QStringLiteral("stop"), QStringLiteral("Spooler") } );
		return configured;
	}
	const bool configured = runSc( { QStringLiteral("config"), QStringLiteral("Spooler"), QStringLiteral("start="), QStringLiteral("auto") } );
	runSc( { QStringLiteral("start"), QStringLiteral("Spooler") } );
	return configured;
#else
	Q_UNUSED(blocked)
	return false;
#endif
}
