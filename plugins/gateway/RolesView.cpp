/*
 * RolesView.cpp - roles of teachers and admins in the Configurator
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
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QVBoxLayout>

#include "CryptoCore.h"
#include "Filesystem.h"
#include "NetworkObjectDirectory.h"
#include "NetworkObjectDirectoryManager.h"
#include "RolesView.h"
#include "ScheduleView.h"
#include "Schedules.h"
#include "SetupCode.h"
#include "VeyonCore.h"


namespace {

const auto RolesBuiltinDirectoryUid = Plugin::Uid( QStringLiteral("14bacaaa-ebe5-449c-b881-5b382f952571") );

enum RoleColumn
{
	RoleColumnName,
	RoleColumnKey,
	RoleColumnRooms,
	RoleColumnPermissions,
	RoleColumnCount
};



QString keyNameFor( const QString& name )
{
	auto key = name.toLower();
	key.replace( QRegularExpression( QStringLiteral("[^a-z0-9]+") ), QStringLiteral("-") );
	key.remove( QRegularExpression( QStringLiteral("^-+|-+$") ) );
	return key.left( 40 );
}

}



RolesView::RolesView( QWidget* parent ) :
	QWidget( parent )
{
	auto layout = new QVBoxLayout( this );

	auto intro = new QLabel( tr( "Give every teacher or admin an own key with limited rights: only the computers of "
								 "their rooms and only the functions you allow. Keys without a role (like the one of "
								 "this computer) keep all rights. Every action is logged with the key used." ) );
	intro->setWordWrap( true );
	layout->addWidget( intro );

	m_table = new QTableWidget( 0, RoleColumnCount );
	m_table->setHorizontalHeaderLabels( { tr( "Name" ), tr( "Key" ), tr( "Rooms" ), tr( "Allowed" ) } );
	m_table->setSelectionBehavior( QAbstractItemView::SelectRows );
	m_table->setSelectionMode( QAbstractItemView::SingleSelection );
	m_table->setEditTriggers( QAbstractItemView::NoEditTriggers );
	m_table->verticalHeader()->hide();
	m_table->setMinimumHeight( 140 );
	m_table->horizontalHeader()->setSectionResizeMode( RoleColumnPermissions, QHeaderView::Stretch );
	layout->addWidget( m_table, 1 );

	auto buttons = new QHBoxLayout;
	auto addButton = new QPushButton( tr( "Add" ) );
	m_editButton = new QPushButton( tr( "Edit" ) );
	m_removeButton = new QPushButton( tr( "Delete" ) );
	m_codeButton = new QPushButton( tr( "Installation code for the teacher..." ) );
	buttons->addWidget( addButton );
	buttons->addWidget( m_editButton );
	buttons->addWidget( m_removeButton );
	buttons->addWidget( m_codeButton );
	buttons->addStretch( 1 );
	layout->addLayout( buttons );

	auto sendBox = new QGroupBox( tr( "Send to all computers" ) );
	auto sendLayout = new QVBoxLayout( sendBox );
	auto sendHint = new QLabel( tr( "The computers only know the roles after they were sent to them. Send again after "
									"every change and when new computers were added." ) );
	sendHint->setWordWrap( true );
	sendLayout->addWidget( sendHint );
	auto sendRow = new QHBoxLayout;
	auto sendButton = new QPushButton( tr( "Send roles now" ) );
	sendRow->addWidget( sendButton );
	m_sendStatus = new QLabel;
	m_sendStatus->setWordWrap( true );
	m_sendStatus->setTextFormat( Qt::PlainText );
	sendRow->addWidget( m_sendStatus, 1 );
	sendLayout->addLayout( sendRow );
	layout->addWidget( sendBox );

	connect( addButton, &QPushButton::clicked, this, &RolesView::addRole );
	connect( m_editButton, &QPushButton::clicked, this, &RolesView::editRole );
	connect( m_removeButton, &QPushButton::clicked, this, &RolesView::removeRole );
	connect( m_codeButton, &QPushButton::clicked, this, &RolesView::showSetupCode );
	connect( sendButton, &QPushButton::clicked, this, &RolesView::sendRoles );
	connect( m_table, &QTableWidget::cellDoubleClicked, this, &RolesView::editRole );
	connect( m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
		const bool selected = selectedRow() >= 0;
		m_editButton->setEnabled( selected );
		m_removeButton->setEnabled( selected );
		m_codeButton->setEnabled( selected );
	} );

	refresh();
}



void RolesView::refresh()
{
	m_roles = AdminRoles::load();
	const auto current = selectedRow();

	m_table->setRowCount( m_roles.size() );
	for( int row = 0; row < m_roles.size(); ++row )
	{
		const auto& role = m_roles.at( row );
		QStringList allowed;
		QStringList allowedShort;
		for( const auto& permission : role.allowed )
		{
			allowed.append( AdminRoles::permissionName( permission ) );
			allowedShort.append( shortPermissionName( permission ) );
		}
		m_table->setItem( row, RoleColumnName, new QTableWidgetItem( role.name ) );
		m_table->setItem( row, RoleColumnKey, new QTableWidgetItem( role.key ) );
		m_table->setItem( row, RoleColumnRooms, new QTableWidgetItem( role.rooms.isEmpty() ? tr( "All rooms" )
																						   : role.rooms.join( QStringLiteral(", ") ) ) );
		auto allowedItem = new QTableWidgetItem( allowed.isEmpty() ? tr( "View only" ) : allowedShort.join( QStringLiteral(", ") ) );
		allowedItem->setToolTip( allowed.join( QLatin1Char('\n') ) );
		m_table->setItem( row, RoleColumnPermissions, allowedItem );
	}
	m_table->resizeColumnsToContents();
	m_table->horizontalHeader()->setSectionResizeMode( RoleColumnPermissions, QHeaderView::Stretch );
	if( current >= 0 && current < m_table->rowCount() )
	{
		m_table->selectRow( current );
	}

	const bool selected = selectedRow() >= 0;
	m_editButton->setEnabled( selected );
	m_removeButton->setEnabled( selected );
	m_codeButton->setEnabled( selected );

	const auto status = Schedules::readStatus()[QString::fromLatin1( Schedules::PushRolesId )].toObject();
	const auto time = QDateTime::fromString( status[QStringLiteral("time")].toString(), Qt::ISODate );
	m_sendStatus->setText( time.isValid() ? tr( "Last sent %1: %2" ).arg( time.toString( QStringLiteral("dd/MM HH:mm") ),
																		  status[QStringLiteral("result")].toString() )
										  : tr( "Not sent yet" ) );
}



bool RolesView::saveRoles( const QList<AdminRoles::Role>& roles )
{
	if( AdminRoles::save( roles ) == false )
	{
		QMessageBox::critical( this, tr( "Admin roles" ), tr( "Could not save the roles (run as administrator)." ) );
		return false;
	}
	refresh();

	if( QMessageBox::question( this, tr( "Admin roles" ), tr( "Send the changed roles to all computers now?" ) ) == QMessageBox::Yes )
	{
		sendRoles();
	}
	return true;
}



void RolesView::addRole()
{
	AdminRoles::Role role;
	role.allowed = QStringList{ QStringLiteral("lock"), QStringLiteral("restrict"), QStringLiteral("apps") };
	RoleDialog dialog( role, true, this );
	if( dialog.exec() != QDialog::Accepted )
	{
		return;
	}

	role = dialog.role();

	// the key pair of the role - the private key stays here to be handed out
	const auto privatePath = VeyonCore::filesystem().privateKeyPath( role.key );
	const auto publicPath = VeyonCore::filesystem().publicKeyPath( role.key );
	if( QFileInfo::exists( publicPath ) == false )
	{
		QApplication::setOverrideCursor( Qt::WaitCursor );
		const auto privateKey = CryptoCore::KeyGenerator().createRSA( CryptoCore::RsaKeySize );
		const auto publicKey = privateKey.toPublicKey();
		const bool written = privateKey.isNull() == false &&
			VeyonCore::filesystem().ensurePathExists( QFileInfo( privatePath ).path() ) &&
			VeyonCore::filesystem().ensurePathExists( QFileInfo( publicPath ).path() ) &&
			privateKey.toPEMFile( privatePath ) && publicKey.toPEMFile( publicPath );
		QApplication::restoreOverrideCursor();
		if( written == false )
		{
			QMessageBox::critical( this, tr( "Admin roles" ), tr( "Could not create the key \"%1\" (run as administrator)." ).arg( role.key ) );
			return;
		}
		QFile::setPermissions( privatePath, QFile::ReadOwner | QFile::ReadUser | QFile::ReadGroup );
		QFile::setPermissions( publicPath, QFile::ReadOwner | QFile::ReadUser | QFile::ReadGroup | QFile::ReadOther );
	}

	QFile publicFile( publicPath );
	if( publicFile.open( QFile::ReadOnly ) )
	{
		role.publicPem = publicFile.readAll();
	}

	auto roles = m_roles;
	roles.append( role );
	saveRoles( roles );
}



void RolesView::editRole()
{
	const auto row = selectedRow();
	if( row < 0 )
	{
		return;
	}

	RoleDialog dialog( m_roles.at( row ), false, this );
	if( dialog.exec() == QDialog::Accepted )
	{
		auto roles = m_roles;
		roles[row] = dialog.role();
		saveRoles( roles );
	}
}



void RolesView::removeRole()
{
	const auto row = selectedRow();
	if( row < 0 )
	{
		return;
	}

	const auto role = m_roles.at( row );
	if( QMessageBox::question( this, tr( "Admin roles" ),
							   tr( "Delete the role \"%1\"? Its key \"%2\" stops working on all computers once the roles "
								   "are sent." ).arg( role.name, role.key ) ) != QMessageBox::Yes )
	{
		return;
	}

	// the key must not stay here: without a role it would have all rights
	for( const auto& path : { VeyonCore::filesystem().privateKeyPath( role.key ), VeyonCore::filesystem().publicKeyPath( role.key ) } )
	{
		QFile::setPermissions( path, QFile::ReadOwner | QFile::WriteOwner );
		QFile::remove( path );
		QDir().rmdir( QFileInfo( path ).path() );
	}

	auto roles = m_roles;
	roles.removeAt( row );
	saveRoles( roles );
}



void RolesView::showSetupCode()
{
	const auto row = selectedRow();
	if( row < 0 )
	{
		return;
	}
	const auto role = m_roles.at( row );

	SetupCode::Options options;
	options.keyNames = QStringList{ role.key };
	options.includePrivateKeys = true;
	options.includeComputers = true;
	QString error;
	const auto code = SetupCode::create( options, &error );
	if( code.isEmpty() )
	{
		QMessageBox::critical( this, tr( "Admin roles" ), error );
		return;
	}

	QDialog dialog( this );
	dialog.setWindowTitle( tr( "Installation code for %1" ).arg( role.name ) );
	dialog.resize( 560, 360 );
	auto layout = new QVBoxLayout( &dialog );
	auto hint = new QLabel( tr( "Install AruniControl on the computer of %1 with this code (save it as aruni-setup.txt next "
								"to the installer, or paste it in the \"Installation\" tab there). The Master there then "
								"works with the rights of the role. Keep the code secret." ).arg( role.name ) );
	hint->setWordWrap( true );
	layout->addWidget( hint );
	auto text = new QPlainTextEdit( code );
	text->setReadOnly( true );
	layout->addWidget( text, 1 );
	auto buttons = new QDialogButtonBox( QDialogButtonBox::Close );
	auto copyButton = buttons->addButton( tr( "Copy" ), QDialogButtonBox::ActionRole );
	auto saveButton = buttons->addButton( tr( "Save as aruni-setup.txt..." ), QDialogButtonBox::ActionRole );
	connect( copyButton, &QPushButton::clicked, &dialog, [code]() { QApplication::clipboard()->setText( code ); } );
	connect( saveButton, &QPushButton::clicked, &dialog, [&dialog, code]() {
		const auto fileName = QFileDialog::getSaveFileName( &dialog, tr( "Save installation code" ),
															QStringLiteral("aruni-setup.txt"), tr( "Text files (*.txt)" ) );
		QFile file( fileName );
		if( fileName.isEmpty() == false && file.open( QFile::WriteOnly | QFile::Truncate ) )
		{
			file.write( code.toUtf8() );
		}
	} );
	connect( buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject );
	layout->addWidget( buttons );
	dialog.exec();
}



void RolesView::sendRoles()
{
	Schedules::requestRun( QString::fromLatin1( Schedules::PushRolesId ) );
	m_sendStatus->setText( tr( "Sending... (within half a minute, this computer's AruniControl service does it)" ) );
}



QString RolesView::shortPermissionName( const QString& permission )
{
	if( permission == QStringLiteral("lock") ) return tr( "Lock & messages" );
	if( permission == QStringLiteral("restrict") ) return tr( "Restrictions" );
	if( permission == QStringLiteral("apps") ) return tr( "Apps & files" );
	if( permission == QStringLiteral("power") ) return tr( "Power" );
	if( permission == QStringLiteral("software") ) return tr( "Software" );
	return permission;
}



int RolesView::selectedRow() const
{
	const auto rows = m_table->selectionModel()->selectedRows();
	return rows.isEmpty() ? -1 : rows.first().row();
}



RoleDialog::RoleDialog( const AdminRoles::Role& role, bool isNew, QWidget* parent ) :
	QDialog( parent ),
	m_role( role ),
	m_isNew( isNew )
{
	setWindowTitle( tr( "Admin role" ) );
	resize( 480, 560 );

	auto layout = new QVBoxLayout( this );
	auto form = new QFormLayout;
	layout->addLayout( form );

	m_name = new QLineEdit( role.name );
	m_name->setPlaceholderText( tr( "e.g. Teacher Lab 1" ) );
	form->addRow( tr( "Name" ), m_name );

	m_key = new QLineEdit( role.key );
	m_key->setReadOnly( isNew == false );
	m_key->setToolTip( tr( "Name of the authentication key of this role" ) );
	form->addRow( tr( "Key" ), m_key );
	if( isNew )
	{
		connect( m_name, &QLineEdit::textChanged, this, [this]( const QString& text ) {
			m_key->setText( keyNameFor( text ) );
		} );
	}

	m_allRooms = new QCheckBox( tr( "All rooms" ) );
	m_allRooms->setChecked( role.rooms.isEmpty() );
	layout->addWidget( m_allRooms );
	m_rooms = new QListWidget;
	const auto rooms = ScheduleRuleDialog::rooms();
	for( const auto& room : rooms )
	{
		auto item = new QListWidgetItem( room, m_rooms );
		item->setFlags( Qt::ItemIsEnabled | Qt::ItemIsUserCheckable );
		item->setCheckState( role.rooms.contains( room ) ? Qt::Checked : Qt::Unchecked );
	}
	m_rooms->setEnabled( role.rooms.isEmpty() == false );
	connect( m_allRooms, &QCheckBox::toggled, m_rooms, [this]( bool all ) { m_rooms->setEnabled( all == false ); } );
	layout->addWidget( m_rooms, 1 );

	auto permissionsBox = new QGroupBox( tr( "Allowed functions (viewing, chat and screenshots are always allowed)" ) );
	auto permissionsLayout = new QVBoxLayout( permissionsBox );
	const auto permissions = AdminRoles::permissions();
	for( const auto& permission : permissions )
	{
		auto checkBox = new QCheckBox( AdminRoles::permissionName( permission ) );
		checkBox->setChecked( role.allowed.contains( permission ) );
		permissionsLayout->addWidget( checkBox );
		m_permissions.append( { permission, checkBox } );
	}
	layout->addWidget( permissionsBox );

	auto buttons = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel );
	connect( buttons, &QDialogButtonBox::accepted, this, &QDialog::accept );
	connect( buttons, &QDialogButtonBox::rejected, this, &QDialog::reject );
	layout->addWidget( buttons );
}



AdminRoles::Role RoleDialog::role() const
{
	auto role = m_role;
	role.name = m_name->text().trimmed();
	role.key = m_key->text().trimmed();
	role.rooms.clear();
	if( m_allRooms->isChecked() == false )
	{
		for( int i = 0; i < m_rooms->count(); ++i )
		{
			if( m_rooms->item( i )->checkState() == Qt::Checked )
			{
				role.rooms.append( m_rooms->item( i )->text() );
			}
		}
	}
	role.hosts = hostsOfRooms( role.rooms );
	role.allowed.clear();
	for( const auto& permission : m_permissions )
	{
		if( permission.second->isChecked() )
		{
			role.allowed.append( permission.first );
		}
	}
	return role;
}



void RoleDialog::accept()
{
	const auto current = role();
	if( current.name.isEmpty() )
	{
		QMessageBox::warning( this, windowTitle(), tr( "Enter a name." ) );
		return;
	}
	if( VeyonCore::isAuthenticationKeyNameValid( current.key ) == false )
	{
		QMessageBox::warning( this, windowTitle(), tr( "The key name may only contain letters, digits, \"-\" and \"_\"." ) );
		return;
	}
	if( m_isNew && ( QFileInfo::exists( VeyonCore::filesystem().privateKeyPath( current.key ) ) ||
					 AdminRoles::roleForKey( current.key ).isValid() ) )
	{
		QMessageBox::warning( this, windowTitle(), tr( "The key \"%1\" already exists - choose another name." ).arg( current.key ) );
		return;
	}
	if( m_allRooms->isChecked() == false && current.rooms.isEmpty() )
	{
		QMessageBox::warning( this, windowTitle(), tr( "Choose at least one room." ) );
		return;
	}
	if( current.rooms.isEmpty() == false && current.hosts.isEmpty() )
	{
		QMessageBox::warning( this, windowTitle(), tr( "The chosen rooms have no computers yet." ) );
		return;
	}
	QDialog::accept();
}



QStringList RoleDialog::hostsOfRooms( const QStringList& rooms )
{
	QStringList hosts;
	if( rooms.isEmpty() )
	{
		return hosts;
	}

	auto directory = VeyonCore::networkObjectDirectoryManager().createDirectory( RolesBuiltinDirectoryUid, nullptr );
	if( directory == nullptr )
	{
		return hosts;
	}
	directory->update();
	const auto objects = directory->queryObjects( NetworkObject::Type::Host, NetworkObject::Attribute::None, {} );
	for( const auto& object : objects )
	{
		const auto parents = directory->queryParents( object );
		const bool inRooms = std::any_of( parents.cbegin(), parents.cend(), [&rooms]( const NetworkObject& parent ) {
			return parent.type() == NetworkObject::Type::Location && rooms.contains( parent.name() );
		} );
		if( inRooms == false )
		{
			continue;
		}
		for( const auto& host : { object.hostAddress(), object.name() } )
		{
			if( host.isEmpty() == false && hosts.contains( host, Qt::CaseInsensitive ) == false )
			{
				hosts.append( host );
			}
		}
	}
	delete directory;
	return hosts;
}
