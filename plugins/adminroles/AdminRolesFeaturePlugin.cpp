/*
 * AdminRolesFeaturePlugin.cpp - distributes the admin roles to the computers
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
#include <QDir>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>

#include "AdminRoles.h"
#include "AdminRolesFeaturePlugin.h"
#include "CryptoCore.h"
#include "Filesystem.h"
#include "VeyonCore.h"
#include "VeyonServerInterface.h"


AdminRolesFeaturePlugin::AdminRolesFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_feature( Feature( QStringLiteral( "AdminRoles" ),
						Feature::Flag::Meta | Feature::Flag::Service,
						Feature::Uid( FeatureUid ),
						Feature::Uid(),
						tr( "Admin roles" ), {},
						tr( "Receives the roles of teachers and admins" ) ) ),
	m_features( { m_feature } )
{
}



const FeatureList& AdminRolesFeaturePlugin::featureList() const
{
	return m_features;
}



bool AdminRolesFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation,
											  const QVariantMap& arguments,
											  const ComputerControlInterfaceList& computerControlInterfaces )
{
	if( featureUid != m_feature.uid() || operation != Operation::Start )
	{
		return false;
	}

	sendFeatureMessage( FeatureMessage{ featureUid, SetPolicy }
							.addArgument( Argument::Policy, arguments.value( QStringLiteral("policy") ).toByteArray() ),
						computerControlInterfaces );
	return true;
}



bool AdminRolesFeaturePlugin::handleFeatureMessage( ComputerControlInterface::Pointer computerControlInterface,
													const FeatureMessage& message )
{
	if( message.featureUid() != m_feature.uid() )
	{
		return false;
	}

	if( static_cast<int>( message.command() ) == Status )
	{
		const auto error = message.argument( Argument::Error ).toString();
		Q_EMIT statusReceived( computerControlInterface, message.argument( Argument::Count ).toInt(), error );

		// a function the key of this Master may not use - say so once, not
		// for each of the computers
		if( VeyonCore::component() == VeyonCore::Component::Master && error.isEmpty() == false &&
			qobject_cast<QApplication*>( QCoreApplication::instance() ) && QApplication::activeWindow() &&
			( m_lastNotice.isValid() == false || m_lastNotice.elapsed() > 5000 ) )
		{
			m_lastNotice.start();
			auto box = new QMessageBox( QMessageBox::Information, tr( "Not allowed" ), error, QMessageBox::Ok,
										QApplication::activeWindow() );
			box->setAttribute( Qt::WA_DeleteOnClose );
			box->setModal( false );
			box->show();
		}
	}
	return true;
}



bool AdminRolesFeaturePlugin::handleFeatureMessage( VeyonServerInterface& server, const MessageContext& messageContext,
													const FeatureMessage& message )
{
	if( message.featureUid() != m_feature.uid() )
	{
		return false;
	}

	QString error;
	if( static_cast<int>( message.command() ) == SetPolicy )
	{
		applyPolicy( message.argument( Argument::Policy ).toByteArray(), error );
	}

	return server.sendFeatureMessageReply( messageContext,
		FeatureMessage{ m_feature.uid(), Status }
			.addArgument( Argument::Count, int( AdminRoles::load().size() ) )
			.addArgument( Argument::Error, error ) );
}



bool AdminRolesFeaturePlugin::applyPolicy( const QByteArray& policy, QString& error )
{
	const auto roles = AdminRoles::fromJson( policy );
	const auto previous = AdminRoles::load();

	for( const auto& role : roles )
	{
		if( role.publicPem.isEmpty() )
		{
			continue;
		}
		if( CryptoCore::PublicKey::fromPEM( QString::fromLatin1( role.publicPem ) ).isPublic() == false )
		{
			error = tr( "The key of the role \"%1\" is damaged." ).arg( role.name );
			return false;
		}

		const auto fileName = VeyonCore::filesystem().publicKeyPath( role.key );
		QFile existing( fileName );
		if( existing.open( QFile::ReadOnly ) && existing.readAll() == role.publicPem )
		{
			continue;
		}
		existing.close();

		VeyonCore::filesystem().ensurePathExists( QFileInfo( fileName ).path() );
		QFile::setPermissions( fileName, QFile::ReadOwner | QFile::WriteOwner );
		QFile::remove( fileName );
		QFile file( fileName );
		if( file.open( QFile::WriteOnly ) == false || file.write( role.publicPem ) != role.publicPem.size() )
		{
			error = tr( "Cannot write the key of the role \"%1\"." ).arg( role.name );
			return false;
		}
		file.close();
		QFile::setPermissions( fileName, QFile::ReadOwner | QFile::ReadUser | QFile::ReadGroup | QFile::ReadOther );
	}

	// keys of deleted roles must not keep working with full rights
	for( const auto& old : previous )
	{
		const bool kept = std::any_of( roles.cbegin(), roles.cend(), [&old]( const AdminRoles::Role& role ) {
			return role.key == old.key;
		} );
		if( kept == false )
		{
			const auto fileName = VeyonCore::filesystem().publicKeyPath( old.key );
			QFile::setPermissions( fileName, QFile::ReadOwner | QFile::WriteOwner );
			QFile::remove( fileName );
			QDir().rmdir( QFileInfo( fileName ).path() );
		}
	}

	if( AdminRoles::save( roles ) == false )
	{
		error = tr( "Cannot save the roles." );
		return false;
	}

	vInfo() << "admin roles updated:" << roles.size();
	return true;
}
