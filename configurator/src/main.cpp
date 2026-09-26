/*
 * main.cpp - main file for Veyon Configurator
 *
 * Copyright (c) 2010-2026 Tobias Junghans <tobydox@veyon.io>
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
#include <QListWidget>
#include <QTimer>
#include <QMessageBox>

#include "VeyonConfiguration.h"
#include "VeyonCore.h"
#include "MainWindow.h"
#include "PlatformCoreFunctions.h"
#include "Logger.h"



int main( int argc, char **argv )
{
	VeyonCore::setupApplicationParameters();

	QApplication app( argc, argv );

	VeyonCore core( &app, VeyonCore::Component::Configurator, QStringLiteral("Configurator") );

	// make sure to run as admin (on macOS the configuration lives in a per-user
	// location, so no privilege elevation is required)
#ifndef Q_OS_MACOS
	if( qEnvironmentVariableIntValue( "VEYON_CONFIGURATOR_NO_ELEVATION" ) == 0 &&
		VeyonCore::platform().coreFunctions().isRunningAsAdmin() == false &&
		app.arguments().size() <= 1 )
	{
		if( VeyonCore::platform().coreFunctions().runProgramAsAdmin( QCoreApplication::applicationFilePath(), {
																	 QStringLiteral("-elevated") } ) )
		{
			return 0;
		}

		QMessageBox::warning( nullptr, MainWindow::tr( "Insufficient privileges" ),
							  MainWindow::tr( "Could not start with administrative privileges. "
											  "Please make sure a sudo-like program is installed "
											  "for your desktop environment! The program will "
											  "be run with normal user privileges.") );
	}
#endif

	if( VeyonConfiguration().isStoreWritable() == false &&
		VeyonCore::config().logLevel() != Logger::LogLevel::Debug )
	{
		QMessageBox::critical(nullptr,
							  MainWindow::tr("Configuration not writable"),
							  MainWindow::tr("The local configuration backend reported that the "
											 "configuration is not writable! Please run AruniControl "
											 "Configurator with higher privileges."));
		return -1;
	}

	// now create the main window
	auto mainWindow = new MainWindow;
	mainWindow->show();

	// documentation screenshots: ARUNI_SCREENSHOT_PAGE selects a page by its
	// title, ARUNI_SCREENSHOT renders the window into a PNG file and quits
	const auto screenshotFile = qEnvironmentVariable( "ARUNI_SCREENSHOT" );
	if( screenshotFile.isEmpty() == false )
	{
		const auto pageSelector = mainWindow->findChild<QListWidget *>( QStringLiteral("pageSelector") );
		const auto items = pageSelector ? pageSelector->findItems( qEnvironmentVariable( "ARUNI_SCREENSHOT_PAGE" ),
																   Qt::MatchStartsWith ) : QList<QListWidgetItem *>{};
		if( items.isEmpty() == false )
		{
			pageSelector->setCurrentItem( items.first() );
		}
		QTimer::singleShot( 1500, mainWindow, [=]() {
			mainWindow->grab().save( screenshotFile );
			QCoreApplication::exit( 0 );
		} );
	}

	return core.exec();
}
