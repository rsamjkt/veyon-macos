import QtQuick
import QtQuick.Controls.Basic as T

T.TextField {
	id: control

	property string iconName: ""
	property string label: ""

	implicitHeight: 52
	leftPadding: iconName.length > 0 ? 48 : 18
	rightPadding: 18
	font.family: Theme.fontFamily
	font.pixelSize: Theme.fontBody
	color: Theme.text
	placeholderTextColor: Theme.textFaint
	selectionColor: Theme.accent
	selectedTextColor: Theme.textOnAccent
	verticalAlignment: TextInput.AlignVCenter

	background: Rectangle {
		radius: Theme.radius
		color: Theme.surfaceAlt
		border.width: control.activeFocus ? 2 : 1
		border.color: control.activeFocus ? Theme.accent : "transparent"
		Behavior on border.color { ColorAnimation { duration: Theme.fast } }

		Icon {
			visible: control.iconName.length > 0
			name: control.iconName
			size: 22
			color: control.activeFocus ? Theme.accent : Theme.textMuted
			anchors.left: parent.left
			anchors.leftMargin: 16
			anchors.verticalCenter: parent.verticalCenter
		}
	}
}
