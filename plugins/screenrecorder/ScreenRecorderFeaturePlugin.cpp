/*
 * ScreenRecorderFeaturePlugin.cpp - implementation of ScreenRecorderFeaturePlugin
 *
 * Copyright (c) 2026 Arunika / AruniControl
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

#include <QMessageBox>

#include "ScreenRecorderFeaturePlugin.h"
#include "ScreenRecording.h"
#include "ComputerControlInterface.h"
#include "VeyonMasterInterface.h"


ScreenRecorderFeaturePlugin::ScreenRecorderFeaturePlugin( QObject* parent ) :
	QObject( parent ),
	m_screenRecorderFeature( Feature( QStringLiteral( "ScreenRecorder" ),
									  Feature::Flag::Mode | Feature::Flag::Master,
									  Feature::Uid( "9f1c2d3e-4b5a-6789-a0b1-c2d3e4f5a6b7" ),
									  Feature::Uid(),
									  tr( "Record screen" ),
									  tr( "Stop recording" ),
									  tr( "Use this function to record the screens of the selected "
										  "computers to video files." ),
									  QStringLiteral(":/screenrecorder/record.png") ) ),
	m_features( { m_screenRecorderFeature } )
{
}



ScreenRecorderFeaturePlugin::~ScreenRecorderFeaturePlugin()
{
	qDeleteAll( m_recordings );
	m_recordings.clear();
}



const FeatureList& ScreenRecorderFeaturePlugin::featureList() const
{
	return m_features;
}



bool ScreenRecorderFeaturePlugin::controlFeature( Feature::Uid featureUid, Operation operation,
												  const QVariantMap& arguments,
												  const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(arguments)

	if( hasFeature( featureUid ) == false )
	{
		return false;
	}

	if( operation == Operation::Start )
	{
		for( const auto& controlInterface : computerControlInterfaces )
		{
			if( m_recordings.contains( controlInterface.data() ) == false )
			{
				m_recordings[controlInterface.data()] = new ScreenRecording( controlInterface, this );
			}
		}
		return true;
	}

	if( operation == Operation::Stop )
	{
		for( const auto& controlInterface : computerControlInterfaces )
		{
			delete m_recordings.take( controlInterface.data() ); // destructor finalises the video file
		}
		return true;
	}

	return false;
}



bool ScreenRecorderFeaturePlugin::startFeature( VeyonMasterInterface& master, const Feature& feature,
												const ComputerControlInterfaceList& computerControlInterfaces )
{
	const bool started = controlFeature( feature.uid(), Operation::Start, {}, computerControlInterfaces );
	if( started )
	{
		QMessageBox::information( master.mainWindow(),
								 tr( "Screen recording started" ),
								 tr( "Recording the screens of %1 computer(s). "
									 "Click the button again to stop. Videos are saved to:\n%2" )
									 .arg( computerControlInterfaces.count() )
									 .arg( ScreenRecording::outputDirectory() ) );
	}
	return started;
}



bool ScreenRecorderFeaturePlugin::stopFeature( VeyonMasterInterface& master, const Feature& feature,
											   const ComputerControlInterfaceList& computerControlInterfaces )
{
	Q_UNUSED(master)
	return controlFeature( feature.uid(), Operation::Stop, {}, computerControlInterfaces );
}
