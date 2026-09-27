/*
 * SetupCode.h - one code that sets up a computer
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
#include <QStringList>

// Everything a new computer needs from the admin computer in one code:
// the public authentication key(s), optionally the private key (for computers
// that act as Master), the list of rooms and computers and the enrollment
// code for roaming laptops. Used by
//   setup.exe /S /SETUP=<code>      (Windows mass installation)
//   veyon-cli gateway setup <code>
// or pasted in the Configurator (Gateway page, "Installation" tab).
//
// Format: "ARUNISETUP1:" + base64url( zlib( JSON ) )
class SetupCode
{
	Q_DECLARE_TR_FUNCTIONS(SetupCode)
public:
	static constexpr auto Prefix = "ARUNISETUP1:";

	struct Options
	{
		QStringList keyNames;			// public keys to include
		bool includePrivateKeys{false};	// makes the code a secret!
		bool includeComputers{false};
		bool includeRoaming{false};
	};

	struct Key
	{
		QString name;
		QByteArray publicPem;
		QByteArray privatePem;
	};

	struct Content
	{
		QString siteName;
		QList<Key> keys;
		QByteArray computers;			// JSON array of the builtin directory
		QString enrollmentCode;
		bool isValid() const
		{
			return keys.isEmpty() == false || computers.isEmpty() == false || enrollmentCode.isEmpty() == false;
		}
	};

	// key names with a public key on this computer
	static QStringList availableKeys();
	static bool hasPrivateKey( const QString& name );

	static QString create( const Options& options, QString* error = nullptr );
	static Content decode( const QString& code );

	// writes keys, switches to key authentication, imports the computers and
	// enrolls as roaming laptop; returns what was done, one line each
	static bool apply( const Content& content, QStringList& report, QString& error );

	// short description of the content, one line each
	static QStringList describe( const Content& content );

};
