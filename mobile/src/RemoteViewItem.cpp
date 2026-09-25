/*
 * RemoteViewItem.cpp - QtQuick item showing (and controlling) a remote screen
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

#include <QDateTime>
#include <QDir>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QSGSimpleTextureNode>

#include <rfb/keysym.h>
#include <rfb/rfbproto.h>

#include "ComputerGridModel.h"
#include "RemoteViewItem.h"
#include "VncConnection.h"
#include "VncView.h"


// bridges the abstract VncView (connection handling, shortcuts, update mode)
// to the QtQuick item
class RemoteVncView : public VncView
{
public:
	RemoteVncView( const ComputerControlInterface::Pointer& computerControlInterface, RemoteViewItem* item ) :
		VncView( computerControlInterface ),
		m_item( item )
	{
		if( auto vnc = connection() )
		{
			QObject::connect( vnc, &VncConnection::imageUpdated, item, [item]() { item->markDirty(); } );
			QObject::connect( vnc, &VncConnection::framebufferSizeChanged, item,
							  [this, item]( int w, int h ) {
								  updateFramebufferSize( w, h );
								  item->onFramebufferSizeChanged( w, h );
							  } );
			QObject::connect( vnc, &VncConnection::cursorShapeUpdated, item,
							  [item]( const QImage& shape, int xh, int yh ) { item->onCursorShapeUpdated( shape, xh, yh ); } );
			QObject::connect( vnc, &VncConnection::stateChanged, item, &RemoteViewItem::statusChanged );
		}
	}

	using VncView::sendShortcut;

protected:
	void updateView( int x, int y, int w, int h ) override
	{
		Q_UNUSED(x) Q_UNUSED(y) Q_UNUSED(w) Q_UNUSED(h)
		m_item->markDirty();
	}

	QSize viewSize() const override
	{
		return m_item->size().toSize();
	}

	void setViewCursor( const QCursor& cursor ) override
	{
		Q_UNUSED(cursor)
	}

	void updateGeometry() override
	{
	}

private:
	RemoteViewItem* m_item;

};



ComputerGridModel* RemoteViewItem::s_model = nullptr;


RemoteViewItem::RemoteViewItem( QQuickItem* parent ) :
	QQuickItem( parent )
{
	setFlag( ItemHasContents, true );
}



RemoteViewItem::~RemoteViewItem() = default;



void RemoteViewItem::setModel( ComputerGridModel* model )
{
	s_model = model;
}



void RemoteViewItem::setComputerUid( const QString& uid )
{
	if( uid == m_computerUid )
	{
		return;
	}

	m_computerUid = uid;
	m_view.reset();
	m_hasImage = false;
	m_framebufferSize = {};

	if( s_model )
	{
		if( const auto controlInterface = s_model->controlInterface( uid ) )
		{
			m_view = std::make_unique<RemoteVncView>( controlInterface, this );
			m_view->setViewOnly( m_controlMode == false );

			if( const auto vnc = m_view->connection() )
			{
				const auto image = vnc->image();
				if( image.isNull() == false )
				{
					onFramebufferSizeChanged( image.width(), image.height() );
					markDirty();
				}
			}
		}
	}

	Q_EMIT computerUidChanged();
	Q_EMIT hasImageChanged();
	Q_EMIT framebufferSizeChanged();
	Q_EMIT statusChanged();
	update();
}



void RemoteViewItem::setControlMode( bool enabled )
{
	if( enabled == m_controlMode )
	{
		return;
	}

	m_controlMode = enabled;
	if( m_view )
	{
		m_view->setViewOnly( enabled == false );
	}

	m_cursorDirty = true;
	update();
	Q_EMIT controlModeChanged();
}



QString RemoteViewItem::status() const
{
	const auto vnc = m_view ? m_view->connection() : nullptr;
	if( vnc == nullptr )
	{
		return QStringLiteral("offline");
	}

	return ComputerGridModel::statusKey( vnc->state() );
}



void RemoteViewItem::movePointer( qreal x, qreal y )
{
	sendPointer( x, y );
}



void RemoteViewItem::pressButton( qreal x, qreal y, int button )
{
	m_buttonMask |= button;
	sendPointer( x, y );
}



void RemoteViewItem::releaseButton( qreal x, qreal y, int button )
{
	m_buttonMask &= ~button;
	sendPointer( x, y );
}



void RemoteViewItem::click( qreal x, qreal y, int button, int count )
{
	sendPointer( x, y );
	for( int i = 0; i < count; ++i )
	{
		pressButton( x, y, button );
		releaseButton( x, y, button );
	}
}



void RemoteViewItem::scroll( qreal x, qreal y, int steps )
{
	// RFB encodes the wheel as buttons 4 (up) and 5 (down)
	const int wheelButton = steps > 0 ? rfbWheelUpMask : rfbWheelDownMask;
	for( int i = 0; i < qAbs( steps ); ++i )
	{
		pressButton( x, y, wheelButton );
		releaseButton( x, y, wheelButton );
	}
}



void RemoteViewItem::typeText( const QString& text )
{
	for( const auto& character : text )
	{
		const auto unicode = character.unicode();
		if( character == QLatin1Char('\n') )
		{
			tapKey( XK_Return );
		}
		else if( character == QLatin1Char('\t') )
		{
			tapKey( XK_Tab );
		}
		else if( unicode < 0x100 )
		{
			// Latin-1 characters map 1:1 to keysyms
			tapKey( unicode );
		}
		else
		{
			tapKey( 0x01000000 | unicode );
		}
	}
}



void RemoteViewItem::pressKey( const QString& keys )
{
	static const QHash<QString, quint32> keyNames{
		{ QStringLiteral("enter"), XK_Return },
		{ QStringLiteral("backspace"), XK_BackSpace },
		{ QStringLiteral("delete"), XK_Delete },
		{ QStringLiteral("tab"), XK_Tab },
		{ QStringLiteral("escape"), XK_Escape },
		{ QStringLiteral("space"), XK_space },
		{ QStringLiteral("up"), XK_Up },
		{ QStringLiteral("down"), XK_Down },
		{ QStringLiteral("left"), XK_Left },
		{ QStringLiteral("right"), XK_Right },
		{ QStringLiteral("home"), XK_Home },
		{ QStringLiteral("end"), XK_End },
		{ QStringLiteral("pageup"), XK_Page_Up },
		{ QStringLiteral("pagedown"), XK_Page_Down },
		{ QStringLiteral("ctrl"), XK_Control_L },
		{ QStringLiteral("alt"), XK_Alt_L },
		{ QStringLiteral("shift"), XK_Shift_L },
		{ QStringLiteral("win"), XK_Super_L },
		{ QStringLiteral("cmd"), XK_Super_L },
		{ QStringLiteral("f1"), XK_F1 }, { QStringLiteral("f2"), XK_F2 }, { QStringLiteral("f3"), XK_F3 },
		{ QStringLiteral("f4"), XK_F4 }, { QStringLiteral("f5"), XK_F5 }, { QStringLiteral("f6"), XK_F6 },
		{ QStringLiteral("f7"), XK_F7 }, { QStringLiteral("f8"), XK_F8 }, { QStringLiteral("f9"), XK_F9 },
		{ QStringLiteral("f10"), XK_F10 }, { QStringLiteral("f11"), XK_F11 }, { QStringLiteral("f12"), XK_F12 },
	};

	// "ctrl+c", "alt+f4", "enter" ...
	QList<quint32> keysyms;
	const auto parts = keys.toLower().split( QLatin1Char('+'), Qt::SkipEmptyParts );
	for( const auto& part : parts )
	{
		if( keyNames.contains( part ) )
		{
			keysyms.append( keyNames[part] );
		}
		else if( part.size() == 1 )
		{
			keysyms.append( part.at(0).unicode() );
		}
	}

	for( const auto keysym : std::as_const(keysyms) )
	{
		sendKey( keysym, true );
	}
	for( auto it = keysyms.crbegin(); it != keysyms.crend(); ++it )
	{
		sendKey( *it, false );
	}
}



void RemoteViewItem::sendShortcut( const QString& shortcut )
{
	if( m_view == nullptr || m_controlMode == false )
	{
		return;
	}

	static const QHash<QString, VncView::Shortcut> shortcuts{
		{ QStringLiteral("ctrl+alt+del"), VncView::ShortcutCtrlAltDel },
		{ QStringLiteral("ctrl+esc"), VncView::ShortcutCtrlEscape },
		{ QStringLiteral("alt+tab"), VncView::ShortcutAltTab },
		{ QStringLiteral("alt+f4"), VncView::ShortcutAltF4 },
		{ QStringLiteral("win+tab"), VncView::ShortcutWinTab },
		{ QStringLiteral("win"), VncView::ShortcutWin },
		{ QStringLiteral("menu"), VncView::ShortcutMenu },
	};

	const auto it = shortcuts.constFind( shortcut.toLower() );
	if( it != shortcuts.constEnd() )
	{
		m_view->sendShortcut( *it );
	}
	else
	{
		pressKey( shortcut );
	}
}



QString RemoteViewItem::saveScreenshot( const QString& directory )
{
	const auto vnc = m_view ? m_view->connection() : nullptr;
	if( vnc == nullptr )
	{
		return {};
	}

	const auto image = vnc->image();
	if( image.isNull() || QDir().mkpath( directory ) == false )
	{
		return {};
	}

	QString name = s_model ? s_model->nameOf( m_computerUid ) : QString{};
	name.replace( QRegularExpression( QStringLiteral("[^A-Za-z0-9_.-]+") ), QStringLiteral("_") );

	const auto fileName = QDir( directory ).filePath(
		QStringLiteral("%1_%2.png").arg( name.isEmpty() ? QStringLiteral("layar") : name,
										 QDateTime::currentDateTime().toString( QStringLiteral("yyyyMMdd-HHmmss") ) ) );

	return image.save( fileName ) ? fileName : QString{};
}



void RemoteViewItem::markDirty()
{
	m_imageDirty = true;
	if( m_hasImage == false )
	{
		m_hasImage = true;
		Q_EMIT hasImageChanged();
	}
	update();
}



void RemoteViewItem::onFramebufferSizeChanged( int width, int height )
{
	const QSize size( width, height );
	if( size != m_framebufferSize )
	{
		m_framebufferSize = size;
		setImplicitSize( width, height );
		Q_EMIT framebufferSizeChanged();
	}
}



void RemoteViewItem::onCursorShapeUpdated( const QImage& shape, int hotX, int hotY )
{
	m_cursorShape = shape;
	m_cursorHotSpot = { hotX, hotY };
	m_cursorDirty = true;
	update();
}



QSGNode* RemoteViewItem::updatePaintNode( QSGNode* oldNode, UpdatePaintNodeData* data )
{
	Q_UNUSED(data)

	const auto vnc = m_view ? m_view->connection() : nullptr;
	if( vnc == nullptr || m_hasImage == false || width() <= 0 || height() <= 0 )
	{
		delete oldNode;
		return nullptr;
	}

	auto node = static_cast<QSGSimpleTextureNode*>( oldNode );
	if( node == nullptr )
	{
		node = new QSGSimpleTextureNode;
		node->setOwnsTexture( true );
		node->setFiltering( QSGTexture::Linear );
		m_imageDirty = true;
		m_cursorDirty = true;
	}

	if( m_imageDirty )
	{
		m_imageDirty = false;
		const auto image = vnc->image();
		if( image.isNull() == false )
		{
			node->setTexture( window()->createTextureFromImage( image ) );
		}
	}

	if( node->texture() == nullptr )
	{
		delete node;
		return nullptr;
	}

	node->setRect( boundingRect() );

	// in control mode the remote cursor is not part of the framebuffer - draw
	// it ourselves at the last pointer position, scaled like the screen
	auto cursorNode = static_cast<QSGSimpleTextureNode*>( node->firstChild() );
	const auto showCursor = m_controlMode && m_cursorShape.isNull() == false && m_framebufferSize.isEmpty() == false;

	if( showCursor == false )
	{
		delete cursorNode;
	}
	else
	{
		if( cursorNode == nullptr )
		{
			cursorNode = new QSGSimpleTextureNode;
			cursorNode->setOwnsTexture( true );
			cursorNode->setFiltering( QSGTexture::Linear );
			node->appendChildNode( cursorNode );
			m_cursorDirty = true;
		}

		if( m_cursorDirty )
		{
			m_cursorDirty = false;
			cursorNode->setTexture( window()->createTextureFromImage( m_cursorShape ) );
		}

		const auto scale = width() / m_framebufferSize.width();
		cursorNode->setRect( m_pointer.x() - m_cursorHotSpot.x() * scale,
							 m_pointer.y() - m_cursorHotSpot.y() * scale,
							 m_cursorShape.width() * scale, m_cursorShape.height() * scale );
	}

	return node;
}



void RemoteViewItem::geometryChange( const QRectF& newGeometry, const QRectF& oldGeometry )
{
	QQuickItem::geometryChange( newGeometry, oldGeometry );
	update();
}



QPoint RemoteViewItem::toFramebuffer( qreal x, qreal y ) const
{
	if( m_framebufferSize.isEmpty() || width() <= 0 || height() <= 0 )
	{
		return {};
	}

	return { qBound( 0, int( x * m_framebufferSize.width() / width() ), m_framebufferSize.width() - 1 ),
			 qBound( 0, int( y * m_framebufferSize.height() / height() ), m_framebufferSize.height() - 1 ) };
}



void RemoteViewItem::sendPointer( qreal x, qreal y )
{
	const auto vnc = m_view ? m_view->connection() : nullptr;
	if( vnc == nullptr || m_controlMode == false )
	{
		return;
	}

	m_pointer = { qBound<qreal>( 0, x, width() ), qBound<qreal>( 0, y, height() ) };
	const auto position = toFramebuffer( x, y );
	vnc->mouseEvent( position.x(), position.y(), m_buttonMask );

	Q_EMIT pointerChanged();
	update();
}



void RemoteViewItem::sendKey( quint32 keysym, bool pressed )
{
	const auto vnc = m_view ? m_view->connection() : nullptr;
	if( vnc && m_controlMode )
	{
		vnc->keyEvent( keysym, pressed );
	}
}



void RemoteViewItem::tapKey( quint32 keysym )
{
	sendKey( keysym, true );
	sendKey( keysym, false );
}
