import QtQuick
import QtQuick.Controls.impl

// Material Symbols (rounded) icon, tinted with a theme color
Item {
	id: icon

	property string name
	property int size: 24
	property color color: Theme.text

	implicitWidth: size
	implicitHeight: size

	IconImage {
		anchors.fill: parent
		source: icon.name.length > 0 ? Qt.resolvedUrl("icons/" + icon.name + ".svg") : ""
		sourceSize: Qt.size(icon.size, icon.size)
		color: icon.color
	}
}
