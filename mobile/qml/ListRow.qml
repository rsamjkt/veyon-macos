import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as T

// Settings-style row: icon, title/subtitle, optional value and chevron
T.ItemDelegate {
	id: row

	property string iconName
	property color iconColor: Theme.accent
	property string title
	property string subtitle
	property string value
	property bool chevron: true
	property bool danger: false
	default property alias trailing: trailingHolder.data

	implicitHeight: Math.max(64, content.implicitHeight + 24)
	padding: 0
	leftPadding: Theme.pad
	rightPadding: Theme.pad

	background: Rectangle {
		color: row.pressed ? Theme.surfaceAlt : "transparent"
		Behavior on color { ColorAnimation { duration: Theme.fast } }
	}

	contentItem: RowLayout {
		id: content
		spacing: 14

		Rectangle {
			visible: row.iconName.length > 0
			width: 40; height: 40; radius: 12
			color: Qt.rgba(row.iconColor.r, row.iconColor.g, row.iconColor.b, Theme.dark ? 0.22 : 0.12)
			Icon { anchors.centerIn: parent; name: row.iconName; color: row.iconColor; size: 22 }
		}

		ColumnLayout {
			Layout.fillWidth: true
			spacing: 2
			AppText {
				Layout.fillWidth: true
				text: row.title
				style: "label"
				font.pixelSize: Theme.fontBody
				color: row.danger ? Theme.danger : Theme.text
				elide: Text.ElideRight
				wrapMode: Text.NoWrap
			}
			AppText {
				Layout.fillWidth: true
				visible: row.subtitle.length > 0
				text: row.subtitle
				style: "caption"
				muted: true
			}
		}

		AppText {
			visible: row.value.length > 0
			text: row.value
			style: "label"
			muted: true
			font.weight: Font.Medium
			wrapMode: Text.NoWrap
		}

		Item {
			id: trailingHolder
			implicitWidth: childrenRect.width
			implicitHeight: childrenRect.height
			visible: children.length > 0
		}

		Icon {
			visible: row.chevron
			name: "chevron_right"
			color: Theme.textFaint
			size: 22
		}
	}
}
