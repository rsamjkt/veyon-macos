/*
 * SiteFilterController.cpp - block websites on computers (SiteFilter plugin)
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

#include <QRegularExpression>
#include <QUrl>

#include "ComputerGridModel.h"
#include "SiteFilterController.h"
#include "SiteFilterFeaturePlugin.h"


SiteFilterController::SiteFilterController( ComputerGridModel* computers, QObject* parent ) :
	FeatureSession( Feature::Uid( SiteFilterFeaturePlugin::FeatureUid ), computers, parent )
{
	// computers with an older AruniControl never answer; a Mac asks its user
	// for the administrator password first, so be patient
	m_timeoutTimer.setSingleShot( true );
	m_timeoutTimer.setInterval( 15000 );
	connect( &m_timeoutTimer, &QTimer::timeout, this, [this]() {
		for( auto& result : m_results )
		{
			if( result.state == QLatin1String("pending") )
			{
				result.state = QStringLiteral("noresponse");
			}
		}
		Q_EMIT resultsChanged();
	} );
}



QVariantList SiteFilterController::presets()
{
	const auto preset = []( const QString& name, const QString& icon, const QStringList& sites ) {
		return QVariantMap{ { QStringLiteral("name"), name }, { QStringLiteral("icon"), icon },
							{ QStringLiteral("sites"), sites } };
	};

	return {
		preset( tr("Media sosial"), QStringLiteral("groups"),
				{ QStringLiteral("facebook.com"), QStringLiteral("instagram.com"), QStringLiteral("tiktok.com"),
				  QStringLiteral("x.com"), QStringLiteral("twitter.com"), QStringLiteral("threads.net"),
				  QStringLiteral("snapchat.com") } ),
		preset( tr("Video"), QStringLiteral("screen_share"),
				{ QStringLiteral("youtube.com"), QStringLiteral("youtu.be"), QStringLiteral("netflix.com"),
				  QStringLiteral("vidio.com"), QStringLiteral("twitch.tv") } ),
		preset( tr("Game online"), QStringLiteral("rocket_launch"),
				{ QStringLiteral("roblox.com"), QStringLiteral("steampowered.com"), QStringLiteral("epicgames.com"),
				  QStringLiteral("miniclip.com"), QStringLiteral("poki.com"), QStringLiteral("friv.com") } ),
		preset( tr("Chat"), QStringLiteral("forum"),
				{ QStringLiteral("web.whatsapp.com"), QStringLiteral("web.telegram.org"), QStringLiteral("discord.com") } ),
	};
}



QVariantList SiteFilterController::results() const
{
	QVariantList list;
	list.reserve( m_results.size() );
	for( const auto& result : m_results )
	{
		list.append( QVariantMap{
			{ QStringLiteral("uid"), result.uid },
			{ QStringLiteral("name"), result.name },
			{ QStringLiteral("state"), result.state },
			{ QStringLiteral("error"), result.error },
			{ QStringLiteral("sites"), result.sites },
			{ QStringLiteral("replied"), result.replied }
		} );
	}
	return list;
}



QStringList SiteFilterController::reportedSites() const
{
	QStringList sites;
	for( const auto& result : m_results )
	{
		for( const auto& site : result.sites )
		{
			if( sites.contains( site ) == false )
			{
				sites.append( site );
			}
		}
	}
	return sites;
}



bool SiteFilterController::isBusy() const
{
	for( const auto& result : m_results )
	{
		if( result.state == QLatin1String("pending") )
		{
			return true;
		}
	}
	return false;
}



QString SiteFilterController::normalizedDomain( const QString& text )
{
	// same rules as SiteFilterFeaturePlugin::normalizedDomain()
	auto value = text.trimmed().toLower();
	if( value.isEmpty() )
	{
		return {};
	}

	if( value.contains( QStringLiteral("://") ) == false )
	{
		value.prepend( QStringLiteral("http://") );
	}
	auto host = QUrl( value ).host();
	if( host.startsWith( QStringLiteral("www.") ) )
	{
		host.remove( 0, 4 );
	}

	static const QRegularExpression domainRX{ QStringLiteral("^([a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?\\.)+[a-z]{2,63}$") };
	return domainRX.match( host ).hasMatch() ? host : QString{};
}



void SiteFilterController::open( const QStringList& uids, const QString& label )
{
	m_uids = uids;
	m_label = label;
	Q_EMIT targetsChanged();

	// ask every computer which sites it blocks right now
	sendToTargets( FeatureMessage{ featureUid(), SiteFilterFeaturePlugin::Query }, QStringLiteral("query") );
}



void SiteFilterController::close()
{
	m_timeoutTimer.stop();
	m_uids.clear();
	m_results.clear();
	m_action.clear();
	Q_EMIT resultsChanged();
}



int SiteFilterController::block( const QStringList& sites )
{
	QStringList domains;
	for( const auto& site : sites )
	{
		const auto domain = normalizedDomain( site );
		if( domain.isEmpty() == false && domains.contains( domain ) == false )
		{
			domains.append( domain );
		}
	}

	if( domains.isEmpty() )
	{
		return 0;
	}

	return sendToTargets( FeatureMessage{ featureUid(), SiteFilterFeaturePlugin::SetSites }
							  .addArgument( SiteFilterFeaturePlugin::Argument::Sites, domains ),
						  QStringLiteral("block") );
}



int SiteFilterController::unblock()
{
	return sendToTargets( FeatureMessage{ featureUid(), SiteFilterFeaturePlugin::SetSites }
							  .addArgument( SiteFilterFeaturePlugin::Argument::Sites, QStringList{} ),
						  QStringLiteral("unblock") );
}



void SiteFilterController::handleMessage( const ComputerControlInterface::Pointer& controlInterface, const FeatureMessage& message )
{
	if( message.command<SiteFilterFeaturePlugin::Command>() != SiteFilterFeaturePlugin::Status )
	{
		return;
	}

	auto result = resultFor( uidOf( controlInterface ) );
	if( result == nullptr )
	{
		return;
	}

	const auto supported = message.argument( SiteFilterFeaturePlugin::Argument::Supported ).toBool();
	result->error = message.argument( SiteFilterFeaturePlugin::Argument::Error ).toString();
	result->sites = message.argument( SiteFilterFeaturePlugin::Argument::Sites ).toStringList();
	result->replied = true;
	if( supported == false )
	{
		result->state = QStringLiteral("unsupported");
	}
	else
	{
		result->state = result->error.isEmpty() ? QStringLiteral("ok") : QStringLiteral("error");
	}

	if( isBusy() == false )
	{
		m_timeoutTimer.stop();
	}
	Q_EMIT resultsChanged();
}



int SiteFilterController::sendToTargets( const FeatureMessage& message, const QString& action )
{
	const auto interfaces = m_uids.isEmpty() ? computers()->visibleControlInterfaces()
											 : computers()->controlInterfaces( m_uids );

	// keep what the computers reported before (e.g. the sites of a query)
	QList<Result> results;
	int sent = 0;
	for( const auto& controlInterface : interfaces )
	{
		Result result;
		result.uid = uidOf( controlInterface );
		result.name = controlInterface->computerName();
		if( const auto previous = resultFor( result.uid ) )
		{
			result.sites = previous->sites;
			result.replied = previous->replied;
		}
		if( send( controlInterface, message ) )
		{
			result.state = QStringLiteral("pending");
			++sent;
		}
		else
		{
			result.state = QStringLiteral("offline");
		}
		results.append( result );
	}

	m_results = results;
	m_action = action;
	if( sent > 0 )
	{
		m_timeoutTimer.start();
	}
	Q_EMIT resultsChanged();
	return sent;
}



SiteFilterController::Result* SiteFilterController::resultFor( const QString& uid )
{
	for( auto& result : m_results )
	{
		if( result.uid == uid )
		{
			return &result;
		}
	}
	return nullptr;
}
