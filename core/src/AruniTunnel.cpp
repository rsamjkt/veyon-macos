/*
 * AruniTunnel.cpp - end-to-end encrypted tunnel between Master and Aruni Gateway
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

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>

#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

#include <memory>

#include "AruniTunnel.h"

namespace AruniTunnel {

namespace {

using PKeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using PKeyCtxPtr = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using CipherCtxPtr = std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)>;

constexpr quint8 FlagPairing = 0x01;
constexpr auto HkdfSalt = "aruni-tunnel-v1";
constexpr auto PairingPrefix = "ARUNI1:";

QByteArray nonceFor( quint64 counter )
{
	QByteArray nonce( 12, 0 );
	qToBigEndian( counter, reinterpret_cast<uchar*>( nonce.data() ) + 4 );
	return nonce;
}

const uchar* bytes( const QByteArray& data )
{
	return reinterpret_cast<const uchar*>( data.constData() );
}

}



KeyPair KeyPair::generate()
{
	return fromPrivateKey( randomBytes( KeySize ) );
}



KeyPair KeyPair::fromPrivateKey( const QByteArray& privateKey )
{
	KeyPair keyPair;
	if( privateKey.size() != KeySize )
	{
		return keyPair;
	}

	PKeyPtr key( EVP_PKEY_new_raw_private_key( EVP_PKEY_X25519, nullptr, bytes( privateKey ), KeySize ), EVP_PKEY_free );
	if( key == nullptr )
	{
		return keyPair;
	}

	QByteArray publicKey( KeySize, 0 );
	size_t length = KeySize;
	if( EVP_PKEY_get_raw_public_key( key.get(), reinterpret_cast<uchar*>( publicKey.data() ), &length ) != 1 ||
		length != KeySize )
	{
		return keyPair;
	}

	keyPair.privateKey = privateKey;
	keyPair.publicKey = publicKey;
	return keyPair;
}



QByteArray randomBytes( int count )
{
	QByteArray data( count, 0 );
	if( RAND_bytes( reinterpret_cast<uchar*>( data.data() ), count ) != 1 )
	{
		qFatal( "AruniTunnel: no secure random numbers available" );
	}
	return data;
}



QByteArray x25519( const QByteArray& privateKey, const QByteArray& publicKey )
{
	if( privateKey.size() != KeySize || publicKey.size() != KeySize )
	{
		return {};
	}

	PKeyPtr ownKey( EVP_PKEY_new_raw_private_key( EVP_PKEY_X25519, nullptr, bytes( privateKey ), KeySize ), EVP_PKEY_free );
	PKeyPtr peerKey( EVP_PKEY_new_raw_public_key( EVP_PKEY_X25519, nullptr, bytes( publicKey ), KeySize ), EVP_PKEY_free );
	if( ownKey == nullptr || peerKey == nullptr )
	{
		return {};
	}

	PKeyCtxPtr ctx( EVP_PKEY_CTX_new( ownKey.get(), nullptr ), EVP_PKEY_CTX_free );
	size_t length = KeySize;
	QByteArray secret( KeySize, 0 );
	if( ctx == nullptr ||
		EVP_PKEY_derive_init( ctx.get() ) != 1 ||
		EVP_PKEY_derive_set_peer( ctx.get(), peerKey.get() ) != 1 ||
		EVP_PKEY_derive( ctx.get(), reinterpret_cast<uchar*>( secret.data() ), &length ) != 1 ||
		length != KeySize )
	{
		return {};
	}

	// reject low-order points (all-zero shared secret)
	if( secret == QByteArray( KeySize, 0 ) )
	{
		return {};
	}

	return secret;
}



QByteArray hkdf( const QByteArray& ikm, const QByteArray& salt, const QByteArray& info, int length )
{
	PKeyCtxPtr ctx( EVP_PKEY_CTX_new_id( EVP_PKEY_HKDF, nullptr ), EVP_PKEY_CTX_free );
	QByteArray output( length, 0 );
	size_t outputLength = size_t( length );

	if( ctx == nullptr ||
		EVP_PKEY_derive_init( ctx.get() ) != 1 ||
		EVP_PKEY_CTX_set_hkdf_md( ctx.get(), EVP_sha256() ) != 1 ||
		EVP_PKEY_CTX_set1_hkdf_salt( ctx.get(), bytes( salt ), int( salt.size() ) ) != 1 ||
		EVP_PKEY_CTX_set1_hkdf_key( ctx.get(), bytes( ikm ), int( ikm.size() ) ) != 1 ||
		EVP_PKEY_CTX_add1_hkdf_info( ctx.get(), bytes( info ), int( info.size() ) ) != 1 ||
		EVP_PKEY_derive( ctx.get(), reinterpret_cast<uchar*>( output.data() ), &outputLength ) != 1 )
	{
		return {};
	}

	return output;
}



QByteArray seal( const QByteArray& key, quint64 counter, const QByteArray& plaintext )
{
	if( key.size() != KeySize )
	{
		return {};
	}

	CipherCtxPtr ctx( EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free );
	const auto nonce = nonceFor( counter );
	QByteArray output( plaintext.size() + TagSize, 0 );
	int length = 0;

	if( ctx == nullptr ||
		EVP_EncryptInit_ex( ctx.get(), EVP_chacha20_poly1305(), nullptr, bytes( key ), bytes( nonce ) ) != 1 ||
		EVP_EncryptUpdate( ctx.get(), reinterpret_cast<uchar*>( output.data() ), &length,
						   bytes( plaintext ), int( plaintext.size() ) ) != 1 ||
		EVP_EncryptFinal_ex( ctx.get(), reinterpret_cast<uchar*>( output.data() ) + length, &length ) != 1 ||
		EVP_CIPHER_CTX_ctrl( ctx.get(), EVP_CTRL_AEAD_GET_TAG, TagSize,
							 output.data() + plaintext.size() ) != 1 )
	{
		return {};
	}

	return output;
}



bool open( const QByteArray& key, quint64 counter, const QByteArray& ciphertext, QByteArray& plaintext )
{
	if( key.size() != KeySize || ciphertext.size() < TagSize )
	{
		return false;
	}

	const auto dataSize = int( ciphertext.size() ) - TagSize;
	CipherCtxPtr ctx( EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free );
	const auto nonce = nonceFor( counter );
	QByteArray tag = ciphertext.right( TagSize );
	plaintext.resize( dataSize );
	int length = 0;

	if( ctx == nullptr ||
		EVP_DecryptInit_ex( ctx.get(), EVP_chacha20_poly1305(), nullptr, bytes( key ), bytes( nonce ) ) != 1 ||
		EVP_DecryptUpdate( ctx.get(), reinterpret_cast<uchar*>( plaintext.data() ), &length,
						   bytes( ciphertext ), dataSize ) != 1 ||
		EVP_CIPHER_CTX_ctrl( ctx.get(), EVP_CTRL_AEAD_SET_TAG, TagSize, tag.data() ) != 1 ||
		EVP_DecryptFinal_ex( ctx.get(), reinterpret_cast<uchar*>( plaintext.data() ) + length, &length ) != 1 )
	{
		plaintext.clear();
		return false;
	}

	return true;
}



QString toBase64Url( const QByteArray& data )
{
	return QString::fromLatin1( data.toBase64( QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals ) );
}



QByteArray fromBase64Url( const QString& text )
{
	return QByteArray::fromBase64( text.trimmed().toLatin1(), QByteArray::Base64UrlEncoding );
}



QByteArray makeFrame( FrameType type, quint32 stream, const QByteArray& payload )
{
	QByteArray frame( 5, 0 );
	frame[0] = char( type );
	qToBigEndian( stream, reinterpret_cast<uchar*>( frame.data() ) + 1 );
	frame.append( payload );
	return frame;
}



bool parseFrame( const QByteArray& frame, FrameType& type, quint32& stream, QByteArray& payload )
{
	if( frame.size() < 5 )
	{
		return false;
	}

	type = FrameType( quint8( frame.at( 0 ) ) );
	stream = qFromBigEndian<quint32>( reinterpret_cast<const uchar*>( frame.constData() ) + 1 );
	payload = frame.mid( 5 );
	return true;
}



bool PairingInfo::isValid() const
{
	return relayUrl.startsWith( QStringLiteral("ws") ) && gatewayId.isEmpty() == false &&
		   gatewayPublicKey.size() == KeySize && token.size() == TokenSize;
}



QString PairingInfo::encode() const
{
	const QJsonObject json{
		{ QStringLiteral("r"), relayUrl },
		{ QStringLiteral("g"), gatewayId },
		{ QStringLiteral("n"), siteName },
		{ QStringLiteral("k"), toBase64Url( gatewayPublicKey ) },
		{ QStringLiteral("t"), toBase64Url( token ) },
	};

	return QLatin1String( PairingPrefix ) + toBase64Url( QJsonDocument( json ).toJson( QJsonDocument::Compact ) );
}



PairingInfo PairingInfo::decode( const QString& text )
{
	auto code = text.trimmed();

	// also accept "arunicontrol://pair?c=ARUNI1:..." links
	const auto linkMarker = QStringLiteral("c=");
	if( code.startsWith( QStringLiteral("arunicontrol:") ) && code.contains( linkMarker ) )
	{
		code = code.mid( code.indexOf( linkMarker ) + linkMarker.size() ).section( QLatin1Char('&'), 0, 0 );
		code = QString::fromUtf8( QByteArray::fromPercentEncoding( code.toUtf8() ) );
	}

	PairingInfo info;
	if( code.startsWith( QLatin1String( PairingPrefix ) ) == false )
	{
		return info;
	}

	const auto json = QJsonDocument::fromJson( fromBase64Url( code.mid( int( qstrlen( PairingPrefix ) ) ) ) ).object();
	info.relayUrl = json[QStringLiteral("r")].toString();
	info.gatewayId = json[QStringLiteral("g")].toString();
	info.siteName = json[QStringLiteral("n")].toString();
	info.gatewayPublicKey = fromBase64Url( json[QStringLiteral("k")].toString() );
	info.token = fromBase64Url( json[QStringLiteral("t")].toString() );
	return info;
}



QByteArray SecureChannel::encrypt( const QByteArray& frame )
{
	return seal( m_sendKey, m_sendCounter++, frame );
}



bool SecureChannel::decrypt( const QByteArray& message, QByteArray& frame )
{
	// messages arrive in order over the relay, so a strictly increasing
	// counter also rejects replayed and reordered messages
	if( open( m_receiveKey, m_receiveCounter, message, frame ) == false )
	{
		return false;
	}
	++m_receiveCounter;
	return true;
}



MasterHandshake::MasterHandshake( const KeyPair& device, const QByteArray& gatewayPublicKey,
								  const QByteArray& pairingToken, const QString& deviceName ) :
	m_device( device ),
	m_ephemeral( KeyPair::generate() ),
	m_gatewayPublicKey( gatewayPublicKey ),
	m_pairingToken( pairingToken ),
	m_deviceName( deviceName )
{
}



QByteArray MasterHandshake::firstMessage()
{
	const bool pairing = m_pairingToken.size() == TokenSize;

	QByteArray message( Magic );
	message.append( char( pairing ? FlagPairing : 0 ) );
	message.append( m_device.publicKey );
	message.append( m_ephemeral.publicKey );

	if( pairing )
	{
		const auto k0 = hkdf( x25519( m_ephemeral.privateKey, m_gatewayPublicKey ), HkdfSalt, "pair", KeySize );
		message.append( seal( k0, 0, m_pairingToken + m_deviceName.left( 64 ).toUtf8() ) );
	}

	m_firstMessage = message;
	return message;
}



bool MasterHandshake::processReply( const QByteArray& reply, SecureChannel& channel, QByteArray& gatewayInfo )
{
	if( reply.size() < KeySize + TagSize || m_firstMessage.isEmpty() )
	{
		return false;
	}

	const auto gatewayEphemeral = reply.left( KeySize );

	const auto ee = x25519( m_ephemeral.privateKey, gatewayEphemeral );
	const auto es = x25519( m_ephemeral.privateKey, m_gatewayPublicKey );
	const auto se = x25519( m_device.privateKey, gatewayEphemeral );
	if( ee.isEmpty() || es.isEmpty() || se.isEmpty() )
	{
		return false;
	}

	const auto salt = QCryptographicHash::hash( QByteArray( m_firstMessage + gatewayEphemeral ), QCryptographicHash::Sha256 );
	const auto keys = hkdf( ee + es + se, salt, "aruni-tunnel-v1 keys", 2 * KeySize );
	if( keys.size() != 2 * KeySize )
	{
		return false;
	}

	const auto masterToGateway = keys.left( KeySize );
	const auto gatewayToMaster = keys.mid( KeySize );

	if( open( gatewayToMaster, 0, reply.mid( KeySize ), gatewayInfo ) == false )
	{
		// the gateway could not derive the same keys - wrong gateway or not authorized
		return false;
	}

	channel = SecureChannel( masterToGateway, gatewayToMaster, 0, 1 );
	return true;
}



bool GatewayHandshake::processFirstMessage( const QByteArray& message, const Authorizer& authorizer,
											const QByteArray& gatewayInfo, QByteArray& reply, SecureChannel& channel )
{
	const auto headerSize = int( qstrlen( Magic ) ) + 1;
	if( message.size() < headerSize + 2 * KeySize || message.startsWith( Magic ) == false )
	{
		return false;
	}

	const auto flags = quint8( message.at( headerSize - 1 ) );
	m_devicePublicKey = message.mid( headerSize, KeySize );
	const auto masterEphemeral = message.mid( headerSize + KeySize, KeySize );

	QByteArray pairingToken;
	QString deviceName;
	if( flags & FlagPairing )
	{
		const auto k0 = hkdf( x25519( m_gateway.privateKey, masterEphemeral ), HkdfSalt, "pair", KeySize );
		QByteArray pairingData;
		if( open( k0, 0, message.mid( headerSize + 2 * KeySize ), pairingData ) == false ||
			pairingData.size() < TokenSize )
		{
			return false;
		}
		pairingToken = pairingData.left( TokenSize );
		deviceName = QString::fromUtf8( pairingData.mid( TokenSize ) );
	}

	if( authorizer( m_devicePublicKey, pairingToken, deviceName ) == false )
	{
		return false;
	}

	const auto ephemeral = KeyPair::generate();
	const auto ee = x25519( ephemeral.privateKey, masterEphemeral );
	const auto es = x25519( m_gateway.privateKey, masterEphemeral );
	const auto se = x25519( ephemeral.privateKey, m_devicePublicKey );
	if( ee.isEmpty() || es.isEmpty() || se.isEmpty() )
	{
		return false;
	}

	const auto salt = QCryptographicHash::hash( QByteArray( message + ephemeral.publicKey ), QCryptographicHash::Sha256 );
	const auto keys = hkdf( ee + es + se, salt, "aruni-tunnel-v1 keys", 2 * KeySize );
	if( keys.size() != 2 * KeySize )
	{
		return false;
	}

	const auto masterToGateway = keys.left( KeySize );
	const auto gatewayToMaster = keys.mid( KeySize );

	reply = ephemeral.publicKey + seal( gatewayToMaster, 0, gatewayInfo );
	channel = SecureChannel( gatewayToMaster, masterToGateway, 1, 0 );
	return true;
}

}
