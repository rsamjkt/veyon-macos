/*
 * ScreenImageProvider.cpp - serves computer thumbnails to QML
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

#include <QUrl>

#include "ComputerGridModel.h"
#include "ScreenImageProvider.h"


ScreenImageProvider::ScreenImageProvider( ComputerGridModel* model ) :
	QQuickImageProvider( QQuickImageProvider::Image ),
	m_model( model )
{
}



QImage ScreenImageProvider::requestImage( const QString& id, QSize* size, const QSize& requestedSize )
{
	// QML percent-encodes the braces of the uid
	const auto uid = QUrl::fromPercentEncoding( id.section( QLatin1Char('/'), 0, 0 ).toUtf8() );
	const auto controlInterface = m_model->controlInterface( uid );

	QImage image;
	if( controlInterface && controlInterface->hasValidFramebuffer() )
	{
		image = controlInterface->scaledFramebuffer();
		if( image.isNull() )
		{
			image = controlInterface->framebuffer();
		}
	}

	if( image.isNull() == false && requestedSize.isValid() &&
		( image.width() > requestedSize.width() || image.height() > requestedSize.height() ) )
	{
		image = image.scaled( requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation );
	}

	if( size )
	{
		*size = image.size();
	}

	return image;
}
