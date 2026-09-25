import QtQuick
import QtQuick.Controls.Basic as T

T.AbstractButton {
	id: control

	property bool selected: false
	property string iconName: ""
	property int count: -1

	checkable: false
	implicitHeight: 36
	implicitWidth: row.implicitWidth + 28
	font.family: Theme.fontFamily
	font.pixelSize: Theme.fontLabel
	font.weight: selected ? Font.DemiBold : Font.Medium

	background: Rectangle {
		radius: height / 2
		color: control.selected ? Theme.text : (control.pressed ? Theme.surfaceHigh : Theme.surface)
		border.width: control.selected ? 0 : 1
		border.color: Theme.border
		Behavior on color { ColorAnimation { duration: Theme.fast } }
	}

	contentItem: Item {
		Row {
			id: row
			anchors.centerIn: parent
			spacing: 6
			Icon {
				visible: control.iconName.length > 0
				name: control.iconName
				size: 18
				color: control.selected ? Theme.background : Theme.textMuted
				anchors.verticalCenter: parent.verticalCenter
			}
			Text {
				text: control.text
				font: control.font
				color: control.selected ? Theme.background : Theme.text
				anchors.verticalCenter: parent.verticalCenter
			}
			Text {
				visible: control.count >= 0
				text: control.count
				font.family: Theme.fontFamily
				font.pixelSize: Theme.fontCaption
				font.weight: Font.Bold
				color: control.selected ? Theme.background : Theme.textMuted
				opacity: 0.8
				anchors.verticalCenter: parent.verticalCenter
			}
		}
	}
}
