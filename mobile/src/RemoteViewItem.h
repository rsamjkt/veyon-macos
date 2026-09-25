/*
 * RemoteViewItem.h - QtQuick item showing (and controlling) a remote screen
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

#include <QElapsedTimer>
#include <QImage>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

#include <memory>

class ComputerGridModel;
class RemoteVncView;

// Renders the framebuffer of one computer as a GPU texture, scaled to the
// item's size - zooming and panning are left to the QML side. All pointer
// coordinates passed in are item coordinates.
class RemoteViewItem : public QQuickItem
{
	Q_OBJECT
	QML_ELEMENT
	Q_PROPERTY(QString computerUid READ computerUid WRITE setComputerUid NOTIFY computerUidChanged)
	Q_PROPERTY(bool controlMode READ controlMode WRITE setControlMode NOTIFY controlModeChanged)
	Q_PROPERTY(QSize framebufferSize READ framebufferSize NOTIFY framebufferSizeChanged)
	Q_PROPERTY(bool hasImage READ hasImage NOTIFY hasImageChanged)
	Q_PROPERTY(QString status READ status NOTIFY statusChanged)
	Q_PROPERTY(QPointF pointer READ pointer NOTIFY pointerChanged)
public:
	explicit RemoteViewItem( QQuickItem* parent = nullptr );
	~RemoteViewItem() override;

	static void setModel( ComputerGridModel* model );

	const QString& computerUid() const
	{
		return m_computerUid;
	}
	void setComputerUid( const QString& uid );

	bool controlMode() const
	{
		return m_controlMode;
	}
	void setControlMode( bool enabled );

	QSize framebufferSize() const
	{
		return m_framebufferSize;
	}

	bool hasImage() const
	{
		return m_hasImage;
	}

	QString status() const;

	// pointer position in item coordinates
	QPointF pointer() const
	{
		return m_pointer;
	}

	Q_INVOKABLE void movePointer( qreal x, qreal y );
	Q_INVOKABLE void pressButton( qreal x, qreal y, int button );
	Q_INVOKABLE void releaseButton( qreal x, qreal y, int button );
	Q_INVOKABLE void click( qreal x, qreal y, int button = 1, int count = 1 );
	Q_INVOKABLE void scroll( qreal x, qreal y, int steps );
	Q_INVOKABLE void typeText( const QString& text );
	Q_INVOKABLE void pressKey( const QString& keys );
	Q_INVOKABLE void sendShortcut( const QString& shortcut );
	Q_INVOKABLE QString saveScreenshot( const QString& directory );

	void markDirty();
	void onFramebufferSizeChanged( int width, int height );
	void onCursorShapeUpdated( const QImage& shape, int hotX, int hotY );

Q_SIGNALS:
	void computerUidChanged();
	void controlModeChanged();
	void framebufferSizeChanged();
	void hasImageChanged();
	void statusChanged();
	void pointerChanged();

protected:
	QSGNode* updatePaintNode( QSGNode* oldNode, UpdatePaintNodeData* data ) override;
	void geometryChange( const QRectF& newGeometry, const QRectF& oldGeometry ) override;

private:
	QPoint toFramebuffer( qreal x, qreal y ) const;
	void sendPointer( qreal x, qreal y );
	void sendKey( quint32 keysym, bool pressed );
	void tapKey( quint32 keysym );

	static ComputerGridModel* s_model;

	QString m_computerUid;
	std::unique_ptr<RemoteVncView> m_view;

	bool m_controlMode{false};
	QSize m_framebufferSize;
	bool m_hasImage{false};
	bool m_imageDirty{false};

	QPointF m_pointer;
	int m_buttonMask{0};

	QImage m_cursorShape;
	QPoint m_cursorHotSpot;
	bool m_cursorDirty{false};

};
