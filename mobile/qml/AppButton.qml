import QtQuick
import QtQuick.Controls.Basic as T

// variants: "filled" (accent), "tonal", "outline", "ghost", "danger"
T.AbstractButton {
	id: control

	property string variant: "filled"
	property string iconName: ""
	property bool busy: false
	property bool compact: false

	readonly property color fg: {
		if (!enabled) return Theme.textFaint
		switch (variant) {
		case "filled": return Theme.textOnAccent
		case "danger": return "#FFFFFF"
		case "tonal": return Theme.dark ? "#FFB27A" : Theme.accentPressed
		default: return Theme.text
		}
	}
	readonly property color bg: {
		if (!enabled) return Theme.surfaceAlt
		switch (variant) {
		case "filled": return pressed ? Theme.accentPressed : Theme.accent
		case "danger": return pressed ? Qt.darker(Theme.danger, 1.15) : Theme.danger
		case "tonal": return pressed ? Qt.darker(Theme.accentSoft, 1.08) : Theme.accentSoft
		case "outline": return pressed ? Theme.surfaceAlt : "transparent"
		default: return pressed ? Theme.surfaceAlt : "transparent"
		}
	}

	implicitHeight: compact ? 40 : 52
	implicitWidth: Math.max(implicitHeight, contentRow.implicitWidth + (compact ? 28 : 40))
	font.family: Theme.fontFamily
	font.pixelSize: compact ? Theme.fontLabel : Theme.fontBody
	font.weight: Font.DemiBold

	background: Rectangle {
		radius: height / 2
		color: control.bg
		border.width: control.variant === "outline" ? 1.5 : 0
		border.color: Theme.border
		Behavior on color { ColorAnimation { duration: Theme.fast } }
	}

	contentItem: Item {
		implicitWidth: contentRow.implicitWidth
		implicitHeight: contentRow.implicitHeight
		Row {
			id: contentRow
			anchors.centerIn: parent
			spacing: 8
			T.BusyIndicator {
				visible: control.busy
				width: 20; height: 20
				anchors.verticalCenter: parent.verticalCenter
				running: control.busy
			}
			Icon {
				visible: control.iconName.length > 0 && !control.busy
				name: control.iconName
				size: control.compact ? 18 : 20
				color: control.fg
				anchors.verticalCenter: parent.verticalCenter
			}
			Text {
				text: control.text
				visible: text.length > 0
				font: control.font
				color: control.fg
				anchors.verticalCenter: parent.verticalCenter
			}
		}
	}

	scale: pressed ? 0.97 : 1
	Behavior on scale { NumberAnimation { duration: Theme.fast; easing.type: Easing.OutCubic } }
}
