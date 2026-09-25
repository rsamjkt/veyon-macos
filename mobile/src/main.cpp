/*
 * main.cpp - AruniControl Mobile entry point
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
#include <QDebug>
#include <QUrl>
#include <QFont>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "ComputerGridModel.h"
#include "MobileApp.h"
#include "RemoteViewItem.h"
#include "ScreenImageProvider.h"
#include "VeyonConfiguration.h"
#include "VeyonCore.h"
#include "VeyonMaster.h"


int main( int argc, char** argv )
{
	VeyonCore::setupApplicationParameters();
#ifndef Q_OS_ANDROID
	// desktop builds are for UI development only - keep their configuration,
	// keys and settings apart from an installed AruniControl on the same machine
	QCoreApplication::setApplicationName( QStringLiteral("AruniControl Mobile") );
#endif

	// feature plugins are QtWidgets based, so this has to be a QApplication
	QApplication app( argc, argv );
	QApplication::setApplicationDisplayName( QStringLiteral("AruniControl") );

	QQuickStyle::setStyle( QStringLiteral("Basic") );

	VeyonCore core( &app, VeyonCore::Component::Master, QStringLiteral("Master") );

	MobileApp::applyDefaults();

	VeyonMaster master( &core );
	MobileApp mobileApp( &master );

	RemoteViewItem::setModel( mobileApp.computers() );

#ifndef Q_OS_ANDROID
	if( const auto keyFile = qEnvironmentVariable( "AC_IMPORT_KEY" ); keyFile.isEmpty() == false )
	{
		const auto error = mobileApp.importKeyFile( QUrl::fromLocalFile( keyFile ), {} );
		qWarning() << "AC_IMPORT_KEY:" << ( error.isEmpty() ? QStringLiteral("ok") : error );
	}
#endif

	QQmlApplicationEngine engine;
	engine.addImageProvider( QStringLiteral("screen"), new ScreenImageProvider( mobileApp.computers() ) );
	engine.rootContext()->setContextProperty( QStringLiteral("App"), &mobileApp );
	engine.loadFromModule( "AruniControl", "Main" );

	if( engine.rootObjects().isEmpty() )
	{
		return -1;
	}

	return core.exec();
}
