/*
 * AruniTunnel.h - end-to-end encrypted tunnel between Master and Aruni Gateway
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

#include <QByteArray>
#include <QString>

#include <functional>

#include "VeyonCore.h"

// A Master outside the school/office network reaches the computers through an
// Aruni Gateway (a client PC on that LAN) via the Aruni Relay. The relay only
// pairs WebSockets and forwards opaque binary messages; everything between
// Master and gateway is protected end to end:
//
// Handshake (Noise-KK style, X25519 + HKDF-SHA256 + ChaCha20-Poly1305):
//   M1  master -> gateway: "AT1" | flags | device_pk | e_m [| seal(k0, token | device name)]
//         k0 = HKDF(DH(e_m, gateway_pk), "pair") - the pairing token (from the
//         QR code) can only be read by the gateway
//   M2  gateway -> master: e_g | seal(k_g2m, gateway info JSON)
//   keys = HKDF(ikm = DH(e_m,e_g) | DH(e_m,gateway_pk) | DH(device,e_g),
//               salt = SHA256(M1 | e_g))
//   -> forward secrecy, the master authenticates the gateway through its static
//      key and the gateway authenticates the device through its static key.
//
// Roaming laptops ("agents") use the same handshake in the master role with
// the agent flag set: they register with the office gateway through an
// enrollment code and keep their session open, and the gateway then opens
// streams in the opposite direction - to the AruniControl server of the laptop.
//
// Afterwards every WebSocket binary message is seal(key, counter, frame) with
// frame = type(1) | stream(4, BE) | payload. Streams carry raw TCP connections
// to computers on the gateway's LAN.
namespace AruniTunnel {

constexpr int KeySize = 32;
constexpr int TagSize = 16;
constexpr int TokenSize = 16;
constexpr int MaxFramePayload = 64 * 1024;
constexpr auto Magic = "AT1";

enum class FrameType : quint8 {
	Open = 1,       // master -> gateway: port(2) | host
	Opened = 2,     // gateway -> master
	Close = 3,      // both directions
	Data = 4,       // both directions
	HostsRequest = 5,
	Hosts = 6,      // gateway -> master: JSON array
	Wake = 7,       // master -> gateway: MAC address (Wake-on-LAN)
	Ping = 8,
	Pong = 9,
	Info = 10,      // gateway -> master: JSON object
	KeyRequest = 11, // master -> gateway, only right after pairing
	Key = 12,       // gateway -> master: JSON {name, pem} of the shared access key
	AgentHello = 13, // agent -> gateway: JSON {name, os, version, addresses}
};

struct VEYON_CORE_EXPORT KeyPair
{
	QByteArray privateKey;
	QByteArray publicKey;

	bool isValid() const
	{
		return privateKey.size() == KeySize && publicKey.size() == KeySize;
	}

	static KeyPair generate();
	static KeyPair fromPrivateKey( const QByteArray& privateKey );
};

VEYON_CORE_EXPORT QByteArray randomBytes( int count );
VEYON_CORE_EXPORT QByteArray x25519( const QByteArray& privateKey, const QByteArray& publicKey );
VEYON_CORE_EXPORT QByteArray hkdf( const QByteArray& ikm, const QByteArray& salt, const QByteArray& info, int length );
VEYON_CORE_EXPORT QByteArray seal( const QByteArray& key, quint64 counter, const QByteArray& plaintext );
VEYON_CORE_EXPORT bool open( const QByteArray& key, quint64 counter, const QByteArray& ciphertext, QByteArray& plaintext );

VEYON_CORE_EXPORT QString toBase64Url( const QByteArray& data );
VEYON_CORE_EXPORT QByteArray fromBase64Url( const QString& text );

VEYON_CORE_EXPORT QByteArray makeFrame( FrameType type, quint32 stream, const QByteArray& payload = {} );
VEYON_CORE_EXPORT bool parseFrame( const QByteArray& frame, FrameType& type, quint32& stream, QByteArray& payload );


// Everything a master needs to reach and pair with a gateway - encoded in the
// QR code / "arunicontrol://pair?c=..." link shown by the Configurator. The
// enrollment code for roaming laptops has the same content with its own prefix
// (and a token that stays valid until the admin renews it).
struct VEYON_CORE_EXPORT PairingInfo
{
	QString relayUrl;
	QString gatewayId;
	QString siteName;
	QByteArray gatewayPublicKey;
	QByteArray token;
	bool enrollment{false};

	bool isValid() const;
	QString encode() const;
	static PairingInfo decode( const QString& text );
};


// Encrypts/decrypts the messages of an established session
class VEYON_CORE_EXPORT SecureChannel
{
public:
	SecureChannel() = default;
	SecureChannel( const QByteArray& sendKey, const QByteArray& receiveKey,
				   quint64 sendCounter = 0, quint64 receiveCounter = 0 ) :
		m_sendKey( sendKey ),
		m_receiveKey( receiveKey ),
		m_sendCounter( sendCounter ),
		m_receiveCounter( receiveCounter )
	{
	}

	bool isValid() const
	{
		return m_sendKey.size() == KeySize && m_receiveKey.size() == KeySize;
	}

	QByteArray encrypt( const QByteArray& frame );
	bool decrypt( const QByteArray& message, QByteArray& frame );

private:
	QByteArray m_sendKey;
	QByteArray m_receiveKey;
	quint64 m_sendCounter{0};
	quint64 m_receiveCounter{0};

};


class VEYON_CORE_EXPORT MasterHandshake
{
public:
	// token/deviceName only for the first connection (pairing), agent for
	// roaming laptops
	MasterHandshake( const KeyPair& device, const QByteArray& gatewayPublicKey,
					 const QByteArray& pairingToken = {}, const QString& deviceName = {}, bool agent = false );

	QByteArray firstMessage();
	bool processReply( const QByteArray& reply, SecureChannel& channel, QByteArray& gatewayInfo );

private:
	KeyPair m_device;
	KeyPair m_ephemeral;
	QByteArray m_gatewayPublicKey;
	QByteArray m_pairingToken;
	QString m_deviceName;
	bool m_agent;
	QByteArray m_firstMessage;

};


class VEYON_CORE_EXPORT GatewayHandshake
{
public:
	// decides whether a device may connect: known device key, or an unused
	// pairing token (which then registers the device); isAgent() is already
	// valid while it runs
	using Authorizer = std::function<bool( const QByteArray& devicePublicKey, const QByteArray& pairingToken, const QString& deviceName )>;

	explicit GatewayHandshake( const KeyPair& gateway ) :
		m_gateway( gateway )
	{
	}

	bool processFirstMessage( const QByteArray& message, const Authorizer& authorizer,
							  const QByteArray& gatewayInfo, QByteArray& reply, SecureChannel& channel );

	const QByteArray& devicePublicKey() const
	{
		return m_devicePublicKey;
	}

	bool isAgent() const
	{
		return m_agent;
	}

private:
	KeyPair m_gateway;
	QByteArray m_devicePublicKey;
	bool m_agent{false};

};

}
