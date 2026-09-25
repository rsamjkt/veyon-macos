import QtQuick
import QtQuick.Controls.Basic as T

T.AbstractButton {
	id: control

	property string iconName
	property color iconColor: Theme.text
	property color backgroundColor: "transparent"
	property int iconSize: 24
	property string tooltip: ""

	implicitWidth: 44
	implicitHeight: 44

	background: Rectangle {
		radius: width / 2
		color: control.pressed ? Qt.rgba(Theme.text.r, Theme.text.g, Theme.text.b, 0.10) : control.backgroundColor
		Behavior on color { ColorAnimation { duration: Theme.fast } }
	}

	contentItem: Item {
		Icon {
			anchors.centerIn: parent
			name: control.iconName
			size: control.iconSize
			color: control.enabled ? control.iconColor : Theme.textFaint
		}
	}

	Accessible.name: tooltip
}
