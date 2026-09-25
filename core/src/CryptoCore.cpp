/*
 * CryptoCore.cpp - core functions for crypto features
 *
 * Copyright (c) 2017-2026 Tobias Junghans <tobydox@veyon.io>
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

#include <QRandomGenerator>
#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QDir>
#include <QPluginLoader>
#endif

#include "CryptoCore.h"

CryptoCore::CryptoCore() :
	m_qcaInitializer(),
	m_defaultPrivateKey()
{
#ifdef Q_OS_ANDROID
	// QCA looks for providers in "<library path>/crypto", but Android keeps all
	// native libraries of an app in one flat directory - register the OpenSSL
	// provider packaged next to the app's libraries explicitly
	QPluginLoader providerLoader( QDir( QCoreApplication::applicationDirPath() ).filePath( QStringLiteral("libqca-ossl.so") ) );
	if( auto plugin = qobject_cast<QCAPlugin*>( providerLoader.instance() ) )
	{
		QCA::insertProvider( plugin->createProvider() );
	}
	else
	{
		vCritical() << "failed to load QCA OpenSSL provider:" << providerLoader.errorString();
	}
#endif

	const auto features = QCA::supportedFeatures();

	vDebug() << "CryptoCore instance created - features supported by QCA" << qcaVersionStr() << features;

	if( features.contains( QStringLiteral( "rsa" ) ) == false )
	{
		qFatal( "CryptoCore: RSA not supported! Please install a QCA plugin which provides RSA support "
				"(e.g. packages such as libqca-qt5-2-plugins or qca-qt5-ossl)." );
	}

	m_defaultPrivateKey = PrivateKey::fromPEMFile( QStringLiteral(":/core/default-pkey.pem") );
}



CryptoCore::~CryptoCore()
{
	vDebug();
}



QByteArray CryptoCore::generateChallenge()
{
	static_assert(ChallengeSize % sizeof(quint32) == 0);

	using ChallengeWords = std::array<quint32, ChallengeSize / sizeof(quint32)>;

	ChallengeWords challenge{};
	QRandomGenerator::system()->fillRange(challenge.data(), challenge.size());

	return QByteArray(reinterpret_cast<const char*>(challenge.data()), ChallengeSize);
}



QString CryptoCore::encryptPassword( const PlaintextPassword& password ) const
{
	return QString::fromLatin1( m_defaultPrivateKey.toPublicKey().
								encrypt( password, DefaultEncryptionAlgorithm ).toByteArray().toHex() );
}



CryptoCore::PlaintextPassword CryptoCore::decryptPassword( const QString& encryptedPassword ) const
{
	PlaintextPassword decryptedPassword;

	if( PrivateKey( m_defaultPrivateKey ).decrypt( QByteArray::fromHex( encryptedPassword.toUtf8() ),
												   &decryptedPassword, DefaultEncryptionAlgorithm ) )
	{
		return decryptedPassword;
	}

	vCritical() << "failed to decrypt password!";

	return {};
}
