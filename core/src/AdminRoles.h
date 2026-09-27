/*
 * AdminRoles.h - limited rights for the keys of teachers and admins
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

#pragma once

#include <QCoreApplication>
#include <QList>
#include <QStringList>

#include "Feature.h"

// Several teachers/admins, each with an own authentication key and limited
// rights: which computers the key may connect to at all and which groups of
// functions it may use. Keys without a role have full rights (as before).
//
// The policy lives in %GLOBALAPPDATA%/roles.json on every computer. The main
// admin sends it to all computers from the Configurator (AdminRoles feature,
// only accepted from a key without role) - including the public keys of the
// roles, so the teachers' keys need no separate installation.
class VEYON_CORE_EXPORT AdminRoles
{
	Q_DECLARE_TR_FUNCTIONS(AdminRoles)
public:
	struct Role
	{
		QString key;			// authentication key name
		QString name;			// e.g. "Guru Lab 1"
		QByteArray publicPem;
		QStringList rooms;		// for display; empty = all computers
		QStringList hosts;		// names/addresses of the computers of the rooms
		QStringList allowed;	// permissions, see permissions()

		bool isValid() const
		{
			return key.isEmpty() == false;
		}
	};

	// groups of functions a role may be allowed
	static QStringList permissions();
	static QString permissionName( const QString& permission );
	// empty: always allowed (viewing, chat, screenshots, ...)
	static QString permissionOf( Feature::Uid featureUid );

	static QString path();
	static QList<Role> load();
	static bool save( const QList<Role>& roles );
	static QByteArray toJson( const QList<Role>& roles );
	static QList<Role> fromJson( const QByteArray& json );

	// role of the key, invalid for keys with full rights
	static Role roleForKey( const QString& keyName );

	static bool isComputerAllowed( const QString& keyName );
	static bool isFeatureAllowed( const QString& keyName, Feature::Uid featureUid );

};
