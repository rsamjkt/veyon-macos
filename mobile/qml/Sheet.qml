import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as T

// Modal bottom sheet; drag down or tap outside to dismiss
T.Drawer {
	id: sheet

	property string title
	property string subtitle
	property string iconName: ""
	property color iconColor: Theme.accent
	default property alias content: body.data
	property alias footer: footerHolder.data
	readonly property real maxContentHeight: (parent ? parent.height : 800) * 0.9

	edge: Qt.BottomEdge
	width: parent ? Math.min(parent.width, 640) : 400
	x: parent ? (parent.width - width) / 2 : 0
	height: Math.min(layout.implicitHeight, maxContentHeight)
	modal: true
	dim: true
	interactive: true
	dragMargin: 0
	// the Basic style pads drawers by the safe area (Qt 6.9+); the sheet sizes
	// itself from its content and adds the bottom inset itself, so the extra
	// padding would clip the last row
	topPadding: 0
	bottomPadding: 0
	leftPadding: 0
	rightPadding: 0

	T.Overlay.modal: Rectangle { color: Theme.scrim }

	enter: Transition { NumberAnimation { property: "position"; to: 1; duration: Theme.normal; easing.type: Easing.OutCubic } }
	exit: Transition { NumberAnimation { property: "position"; to: 0; duration: Theme.normal; easing.type: Easing.InCubic } }

	background: Item {
		Rectangle {
			anchors.fill: parent
			anchors.bottomMargin: -Theme.radiusSheet
			radius: Theme.radiusSheet
			color: Theme.surface
		}
	}

	contentItem: ColumnLayout {
		id: layout
		spacing: 0

		Rectangle {
			Layout.alignment: Qt.AlignHCenter
			Layout.topMargin: 10
			width: 40; height: 5; radius: 3
			color: Theme.border
		}

		RowLayout {
			visible: sheet.title.length > 0
			Layout.fillWidth: true
			Layout.leftMargin: Theme.padLarge
			Layout.rightMargin: Theme.padLarge
			Layout.topMargin: 14
			Layout.bottomMargin: 6
			spacing: 14

			Rectangle {
				visible: sheet.iconName.length > 0
				width: 44; height: 44; radius: 14
				color: Qt.rgba(sheet.iconColor.r, sheet.iconColor.g, sheet.iconColor.b, Theme.dark ? 0.22 : 0.13)
				Icon { anchors.centerIn: parent; name: sheet.iconName; color: sheet.iconColor; size: 24 }
			}
			ColumnLayout {
				Layout.fillWidth: true
				spacing: 2
				AppText { text: sheet.title; style: "heading"; Layout.fillWidth: true }
				AppText { visible: sheet.subtitle.length > 0; text: sheet.subtitle; style: "label"; muted: true; font.weight: Font.Normal; Layout.fillWidth: true }
			}
		}

		Flickable {
			id: flick
			Layout.fillWidth: true
			Layout.fillHeight: true
			Layout.preferredHeight: body.implicitHeight + 24
			contentHeight: body.implicitHeight + 24
			clip: true
			boundsBehavior: Flickable.StopAtBounds
			interactive: contentHeight > height

			ColumnLayout {
				id: body
				x: Theme.padLarge
				y: 12
				width: flick.width - 2 * Theme.padLarge
				spacing: 12
			}
		}

		Item {
			id: footerHolder
			Layout.fillWidth: true
			Layout.leftMargin: Theme.padLarge
			Layout.rightMargin: Theme.padLarge
			implicitHeight: childrenRect.height
			visible: children.length > 0
		}

		Item {
			Layout.fillWidth: true
			implicitHeight: Theme.pad + (sheet.T.ApplicationWindow.window ? sheet.T.ApplicationWindow.window.safeBottom : 0)
		}
	}
}
