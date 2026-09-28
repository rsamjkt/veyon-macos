/*
 * LabCleanFeaturePlugin.cpp - clean student accounts at every restart
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
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QRegularExpression>
#include <QTimer>
#include <QVBoxLayout>

#if defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <sys/sysctl.h>
#include <sys/time.h>
#endif

#include "AccessLog.h"
#include "Filesystem.h"
#include "LabCleanFeaturePlugin.h"
#include "PlatformUserFunctions.h"
#include "VeyonCore.h"
#include "VeyonMasterInterface.h"
#include "VeyonServerInterface.h"


namespace {

// "BUILTIN\Administrators"
const auto AdministratorsSid = QStringLiteral("S-1-5-32-544");

}



LabCleanFeaturePlugin::LabCleanFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_feature( Feature( QStringLiteral( "LabClean" ),
						Feature::Flag::Action | Feature::Flag::AllComponents,
						Feature::Uid( FeatureUid ),
						Feature::Uid(),
						tr( "Lab clean mode" ), {},
						tr( "At every restart the files of the student accounts are moved away, so every lesson starts "
							"with a clean computer. Kept for some days, nothing is lost." ),
						QStringLiteral(":/labclean/labclean.png") ) ),
	m_features( { m_feature } )
{
	// Windows: the service starts at boot, before anybody logs on. macOS has
	// no service - the server starts when the user logs on.
#if defined(Q_OS_WIN)
	const auto cleaningComponent = VeyonCore::Component::Service;
#else
	const auto cleaningComponent = VeyonCore::Component::Server;
#endif
	if( VeyonCore::component() == cleaningComponent )
	{
		QTimer::singleShot( 1000, this, []() {
			const auto settings = loadSettings();
			if( settings.enabled )
			{
				QString error;
				const auto moved = clean( settings, true, error );
				if( moved > 0 || error.isEmpty() == false )
				{
					vInfo() << "lab clean mode:" << moved << "entries moved" << error;
				}
			}
		} );
	}
}



const FeatureList& LabCleanFeaturePlugin::featureList() const
{
	return m_features;
}



QStringList LabCleanFeaturePlugin::defaultFolders()
{
	return { QStringLiteral("Desktop"), QStringLiteral("Downloads"), QStringLiteral("Documents"),
			 QStringLiteral("Pictures"), QStringLiteral("Videos"), QStringLiteral("Music") };
}



QString LabCleanFeaturePlugin::folderDisplayName( const QString& folder )
{
	if( folder == QStringLiteral("Desktop") ) return tr( "Desktop" );
	if( folder == QStringLiteral("Downloads") ) return tr( "Downloads" );
	if( folder == QStringLiteral("Documents") ) return tr( "Documents" );
	if( folder == QStringLiteral("Pictures") ) return tr( "Pictures" );
	if( folder == QStringLiteral("Videos") ) return tr( "Videos" );
	if( folder == QStringLiteral("Music") ) return tr( "Music" );
	return folder;
}



QJsonObject LabCleanFeaturePlugin::toJson( const Settings& settings )
{
	return {
		{ QStringLiteral("enabled"), settings.enabled },
		{ QStringLiteral("users"), QJsonArray::fromStringList( settings.users ) },
		{ QStringLiteral("folders"), QJsonArray::fromStringList( settings.folders ) },
		{ QStringLiteral("keepDays"), settings.keepDays },
	};
}



LabCleanFeaturePlugin::Settings LabCleanFeaturePlugin::fromJson( const QJsonObject& json )
{
	Settings settings;
	settings.enabled = json[QStringLiteral("enabled")].toBool();
	for( const auto& user : json[QStringLiteral("users")].toVariant().toStringList() )
	{
		const auto name = user.trimmed();
		// plain account names only - they become parts of paths
		if( name.isEmpty() == false && name.contains( QRegularExpression( QStringLiteral("[\\\\/:*?\"<>|]|\\.\\.") ) ) == false )
		{
			settings.users.append( name );
		}
	}
	for( const auto& folder : json[QStringLiteral("folders")].toVariant().toStringList() )
	{
		if( defaultFolders().contains( folder ) )
		{
			settings.folders.append( folder );
		}
	}
	settings.keepDays = qBound( 1, json[QStringLiteral("keepDays")].toInt( 7 ), 90 );
	if( settings.users.isEmpty() || settings.folders.isEmpty() )
	{
		settings.enabled = false;
	}
	return settings;
}



QString LabCleanFeaturePlugin::statePath()
{
	return VeyonCore::filesystem().expandPath( QStringLiteral("%GLOBALAPPDATA%/labclean.json") );
}



QString LabCleanFeaturePlugin::quarantineDirectory()
{
	return VeyonCore::filesystem().expandPath( QStringLiteral("%GLOBALAPPDATA%/labclean") );
}



LabCleanFeaturePlugin::Settings LabCleanFeaturePlugin::loadSettings()
{
	QFile file( statePath() );
	if( file.open( QFile::ReadOnly ) == false )
	{
		return {};
	}
	return fromJson( QJsonDocument::fromJson( file.readAll() ).object()[QStringLiteral("settings")].toObject() );
}



void LabCleanFeaturePlugin::saveSettings( const Settings& settings )
{
	QJsonObject state;
	{
		QFile file( statePath() );
		if( file.open( QFile::ReadOnly ) )
		{
			state = QJsonDocument::fromJson( file.readAll() ).object();
		}
	}
	state[QStringLiteral("settings")] = toJson( settings );
	QDir().mkpath( QFileInfo( statePath() ).absolutePath() );
	QFile file( statePath() );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) )
	{
		file.write( QJsonDocument( state ).toJson() );
	}
	setFastStartupDisabled( settings.enabled );
}



bool LabCleanFeaturePlugin::isAdministrator( const QString& user )
{
#if defined(Q_OS_WIN)
	auto& userFunctions = VeyonCore::platform().userFunctions();
	const auto groups = userFunctions.groupsOfUser( user, false );
	for( const auto& group : groups )
	{
		if( userFunctions.userGroupSecurityIdentifier( group ) == AdministratorsSid ||
			group.compare( QStringLiteral("Administrators"), Qt::CaseInsensitive ) == 0 )
		{
			return true;
		}
	}
	return user.compare( QStringLiteral("Administrator"), Qt::CaseInsensitive ) == 0;
#else
	const auto groups = VeyonCore::platform().userFunctions().groupsOfUser( user, false );
	return groups.contains( QStringLiteral("admin") ) || user == QStringLiteral("root");
#endif
}



QString LabCleanFeaturePlugin::profilePath( const QString& user )
{
#if defined(Q_OS_WIN)
	const auto systemDrive = qEnvironmentVariable( "SystemDrive", QStringLiteral("C:") );
	const auto path = systemDrive + QStringLiteral("/Users/") + user;
#else
	// only the account the server runs for
	const auto current = VeyonCore::platform().userFunctions().queryCurrentUserProperty( PlatformUserFunctions::UserProperty::LoginName );
	const auto path = current.compare( user, Qt::CaseInsensitive ) == 0 ? QDir::homePath() : QString{};
#endif
	return path.isEmpty() == false && QFileInfo( path ).isDir() ? path : QString{};
}



QString LabCleanFeaturePlugin::bootId()
{
#if defined(Q_OS_WIN)
	const auto boot = QDateTime::currentDateTimeUtc().addMSecs( -qint64( GetTickCount64() ) );
	// to the minute - the tick count and the clock never match exactly
	return boot.toString( QStringLiteral("yyyy-MM-ddTHH:mm") );
#elif defined(Q_OS_MACOS)
	struct timeval boot{};
	size_t size = sizeof( boot );
	if( sysctlbyname( "kern.boottime", &boot, &size, nullptr, 0 ) == 0 )
	{
		return QString::number( boot.tv_sec );
	}
	return {};
#else
	return {};
#endif
}



void LabCleanFeaturePlugin::setFastStartupDisabled( bool disabled )
{
#if defined(Q_OS_WIN)
	// with fast startup, "shut down" hibernates the system and the service
	// does not start again - the accounts would not be cleaned
	QSettings power( QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Power"),
					 QSettings::NativeFormat );
	power.setValue( QStringLiteral("HiberbootEnabled"), disabled ? 0 : 1 );
#else
	Q_UNUSED(disabled)
#endif
}



int LabCleanFeaturePlugin::clean( const Settings& settings, bool onlyOncePerBoot, QString& error )
{
	QJsonObject state;
	{
		QFile file( statePath() );
		if( file.open( QFile::ReadOnly ) )
		{
			state = QJsonDocument::fromJson( file.readAll() ).object();
		}
	}

	const auto boot = bootId();
	if( onlyOncePerBoot && boot.isEmpty() == false && state[QStringLiteral("boot")].toString() == boot )
	{
		return 0;
	}

	const auto stamp = QDateTime::currentDateTime().toString( QStringLiteral("yyyy-MM-dd_HHmmss") );
	int moved = 0;
	QStringList problems;

	for( const auto& user : settings.users )
	{
		if( isAdministrator( user ) )
		{
			problems.append( tr( "%1 is an administrator - skipped" ).arg( user ) );
			continue;
		}
		const auto profile = profilePath( user );
		if( profile.isEmpty() )
		{
			continue;
		}

		for( const auto& folder : settings.folders )
		{
			const QDir source( QDir( profile ).filePath( folder ) );
			const QFileInfo sourceInfo( source.path() );
			// redirected folders (OneDrive, network) are left alone
			if( sourceInfo.isDir() == false || sourceInfo.isSymLink() || sourceInfo.isJunction() )
			{
				continue;
			}
			const auto entries = source.entryInfoList( QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot );
			if( entries.isEmpty() )
			{
				continue;
			}

			const QDir target( QDir( quarantineDirectory() ).filePath( stamp + QLatin1Char('/') + user + QLatin1Char('/') + folder ) );
			QDir().mkpath( target.path() );
			for( const auto& entry : entries )
			{
				const auto name = entry.fileName();
				if( name.compare( QStringLiteral("desktop.ini"), Qt::CaseInsensitive ) == 0 ||
					name == QStringLiteral(".localized") || name == QStringLiteral(".DS_Store") )
				{
					continue;
				}
				// a move within the drive - never a copy and delete, a file in
				// use or on another drive simply stays where it is
				if( QDir().rename( entry.absoluteFilePath(), target.filePath( name ) ) )
				{
					++moved;
				}
			}
		}
	}

	prune( settings.keepDays );

	state[QStringLiteral("boot")] = boot;
	state[QStringLiteral("lastClean")] = QDateTime::currentDateTime().toString( Qt::ISODate );
	state[QStringLiteral("lastMoved")] = moved;
	QDir().mkpath( QFileInfo( statePath() ).absolutePath() );
	QFile file( statePath() );
	if( file.open( QFile::WriteOnly | QFile::Truncate ) )
	{
		file.write( QJsonDocument( state ).toJson() );
	}

	AccessLog::append( QStringLiteral("lab_clean"), {}, settings.users.join( QStringLiteral(", ") ),
					   { { QStringLiteral("moved"), moved } } );

	error = problems.join( QStringLiteral("; ") );
	return moved;
}



void LabCleanFeaturePlugin::prune( int keepDays )
{
	const QDir base( quarantineDirectory() );
	const auto oldest = QDateTime::currentDateTime().addDays( -keepDays );
	const auto folders = base.entryList( QDir::Dirs | QDir::NoDotAndDotDot );
	for( const auto& folder : folders )
	{
		const auto time = QDateTime::fromString( folder, QStringLiteral("yyyy-MM-dd_HHmmss") );
		if( time.isValid() && time < oldest )
		{
			QDir( base.filePath( folder ) ).removeRecursively();
		}
	}
}



bool LabCleanFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation, const QVariantMap& arguments,
											const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( featureUid != m_feature.uid() )
	{
		return false;
	}

	switch( operation )
	{
	case Operation::Initialize:
		sendFeatureMessage( FeatureMessage{ featureUid, Query }, computerControlInterfaces );
		return true;
	case Operation::Start:
		if( arguments.value( QStringLiteral("cleanNow") ).toBool() )
		{
			sendFeatureMessage( FeatureMessage{ featureUid, CleanNow }, computerControlInterfaces );
		}
		else
		{
			sendFeatureMessage( FeatureMessage{ featureUid, Configure }
									.addArgument( Argument::Settings, QJsonDocument( QJsonObject::fromVariantMap( arguments ) ).toJson() ),
								computerControlInterfaces );
		}
		return true;
	case Operation::Stop:
		sendFeatureMessage( FeatureMessage{ featureUid, Configure }.addArgument( Argument::Settings, QByteArray( "{}" ) ),
							computerControlInterfaces );
		return true;
	default:
		break;
	}
	return false;
}



bool LabCleanFeaturePlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
										  const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( feature.uid() != m_feature.uid() || computerControlInterfaces.isEmpty() )
	{
		return false;
	}

	QDialog dialog( master.mainWindow() );
	dialog.setWindowTitle( tr( "Lab clean mode" ) );
	dialog.resize( 520, 480 );
	auto layout = new QVBoxLayout( &dialog );

	auto intro = new QLabel( tr( "At every restart of the %n selected computer(s), the files in the chosen folders of the "
								 "student accounts are moved to a safe place and deleted from there after the chosen "
								 "days. Programs and settings stay as they are. Accounts of administrators are never "
								 "cleaned.", nullptr, int( computerControlInterfaces.size() ) ) );
	intro->setWordWrap( true );
	layout->addWidget( intro );

	QSettings stored( QStringLiteral("AruniControl"), QStringLiteral("LabClean") );
	auto form = new QFormLayout;
	auto users = new QLineEdit( stored.value( QStringLiteral("users"), QStringLiteral("siswa") ).toString() );
	users->setPlaceholderText( tr( "e.g. siswa, murid" ) );
	form->addRow( tr( "Student accounts" ), users );
	auto keepDays = new QSpinBox;
	keepDays->setRange( 1, 90 );
	keepDays->setValue( stored.value( QStringLiteral("keepDays"), 7 ).toInt() );
	keepDays->setSuffix( tr( " days" ) );
	form->addRow( tr( "Keep the files for" ), keepDays );
	layout->addLayout( form );

	auto foldersBox = new QGroupBox( tr( "Folders to clean" ) );
	auto foldersLayout = new QGridLayout( foldersBox );
	QList<QPair<QString, QCheckBox*>> folderBoxes;
	const auto storedFolders = stored.value( QStringLiteral("folders"), QStringList{ QStringLiteral("Desktop"), QStringLiteral("Downloads"), QStringLiteral("Documents") } ).toStringList();
	const auto folders = defaultFolders();
	for( int i = 0; i < folders.size(); ++i )
	{
		auto checkBox = new QCheckBox( folderDisplayName( folders.at( i ) ) );
		checkBox->setChecked( storedFolders.contains( folders.at( i ) ) );
		foldersLayout->addWidget( checkBox, i / 3, i % 3 );
		folderBoxes.append( { folders.at( i ), checkBox } );
	}
	layout->addWidget( foldersBox );

	auto status = new QLabel( tr( "Checking the current state..." ) );
	status->setWordWrap( true );
	status->setStyleSheet( QStringLiteral("color: gray;") );
	layout->addWidget( status );

	auto buttons = new QDialogButtonBox( QDialogButtonBox::Cancel );
	auto enableButton = buttons->addButton( tr( "Turn on" ), QDialogButtonBox::AcceptRole );
	auto disableButton = buttons->addButton( tr( "Turn off" ), QDialogButtonBox::ActionRole );
	auto cleanButton = buttons->addButton( tr( "Clean now" ), QDialogButtonBox::ActionRole );
	layout->addWidget( buttons );

	int answers = 0;
	int enabled = 0;
	const auto connection = connect( this, &LabCleanFeaturePlugin::statusReceived, &dialog,
		[&]( ComputerControlInterface::Pointer, const QJsonObject& settings, const QString& lastClean, int moved, const QString& error ) {
			++answers;
			enabled += fromJson( settings ).enabled ? 1 : 0;
			status->setText( tr( "%1 of %2 computers answered, on for %3." ).arg( answers ).arg( computerControlInterfaces.size() ).arg( enabled ) +
							 ( lastClean.isEmpty() ? QString{} : QLatin1Char(' ') + tr( "Last cleaning: %1 (%2 items)" ).arg( lastClean, QString::number( moved ) ) ) +
							 ( error.isEmpty() ? QString{} : QStringLiteral("\n") + error ) );
		} );
	controlFeature( m_feature.uid(), Operation::Initialize, {}, computerControlInterfaces );

	const auto currentSettings = [&]() {
		QStringList folderList;
		for( const auto& folder : folderBoxes )
		{
			if( folder.second->isChecked() )
			{
				folderList.append( folder.first );
			}
		}
		stored.setValue( QStringLiteral("users"), users->text() );
		stored.setValue( QStringLiteral("keepDays"), keepDays->value() );
		stored.setValue( QStringLiteral("folders"), folderList );
		return QVariantMap{
			{ QStringLiteral("enabled"), true },
			{ QStringLiteral("users"), users->text().split( QRegularExpression( QStringLiteral("[,;\\s]+") ), Qt::SkipEmptyParts ) },
			{ QStringLiteral("folders"), folderList },
			{ QStringLiteral("keepDays"), keepDays->value() },
		};
	};

	connect( enableButton, &QPushButton::clicked, &dialog, [&]() {
		const auto settings = currentSettings();
		if( settings.value( QStringLiteral("users") ).toStringList().isEmpty() || settings.value( QStringLiteral("folders") ).toStringList().isEmpty() )
		{
			QMessageBox::warning( &dialog, dialog.windowTitle(), tr( "Enter the student accounts and choose at least one folder." ) );
			return;
		}
		controlFeature( m_feature.uid(), Operation::Start, settings, computerControlInterfaces );
		dialog.accept();
	} );
	connect( disableButton, &QPushButton::clicked, &dialog, [&]() {
		controlFeature( m_feature.uid(), Operation::Stop, {}, computerControlInterfaces );
		dialog.accept();
	} );
	connect( cleanButton, &QPushButton::clicked, &dialog, [&]() {
		if( QMessageBox::question( &dialog, dialog.windowTitle(),
								   tr( "Move the files of the student accounts away now? Files that are open stay where they are." ) )
			== QMessageBox::Yes )
		{
			controlFeature( m_feature.uid(), Operation::Start, { { QStringLiteral("cleanNow"), true } }, computerControlInterfaces );
		}
	} );
	connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );

	// the "accept" role button must not close the dialog before validating
	disconnect( buttons, &QDialogButtonBox::accepted, nullptr, nullptr );

	dialog.exec();
	disconnect( connection );
	return true;
}



bool LabCleanFeaturePlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
												  const FeatureMessage& message )
{
	if( message.featureUid() != m_feature.uid() )
	{
		return false;
	}
	if( static_cast<int>( message.command() ) == Status )
	{
		Q_EMIT statusReceived( computerControlInterface,
							   QJsonDocument::fromJson( message.argument( Argument::Settings ).toByteArray() ).object(),
							   message.argument( Argument::LastClean ).toString(), message.argument( Argument::Moved ).toInt(),
							   message.argument( Argument::Error ).toString() );
	}
	return true;
}



bool LabCleanFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
												  const FeatureMessage& message )
{
	if( message.featureUid() != m_feature.uid() )
	{
		return false;
	}

	QString error;
	switch( static_cast<int>( message.command() ) )
	{
	case Configure:
	{
		const auto settings = fromJson( QJsonDocument::fromJson( message.argument( Argument::Settings ).toByteArray() ).object() );
		saveSettings( settings );
		vInfo() << "lab clean mode" << settings.enabled << settings.users << settings.folders;
		break;
	}
	case CleanNow:
	{
		const auto settings = loadSettings();
		if( settings.users.isEmpty() )
		{
			error = tr( "Turn on the lab clean mode first" );
		}
		else
		{
			clean( settings, false, error );
		}
		break;
	}
	default:
		break;
	}

	QJsonObject state;
	QFile file( statePath() );
	if( file.open( QFile::ReadOnly ) )
	{
		state = QJsonDocument::fromJson( file.readAll() ).object();
	}

	return server.sendFeatureMessageReply( messageContext, FeatureMessage{ m_feature.uid(), Status }
		.addArgument( Argument::Settings, QJsonDocument( state[QStringLiteral("settings")].toObject() ).toJson() )
		.addArgument( Argument::LastClean, state[QStringLiteral("lastClean")].toString() )
		.addArgument( Argument::Moved, state[QStringLiteral("lastMoved")].toInt() )
		.addArgument( Argument::Error, error ) );
}
