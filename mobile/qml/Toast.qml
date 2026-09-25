import QtQuick

// Transient message at the bottom of the screen
Rectangle {
	id: toast

	property string kind: "info"   // info, success, error

	function show(message, messageKind) {
		label.text = message
		kind = messageKind || "info"
		hideTimer.restart()
		state = "shown"
	}

	anchors.horizontalCenter: parent.horizontalCenter
	width: Math.min(parent.width - 2 * Theme.pad, row.implicitWidth + 36)
	height: Math.max(52, label.implicitHeight + 28)
	radius: 18
	color: Theme.dark ? "#F4EFE9" : "#26221F"
	y: parent.height
	opacity: 0
	z: 1000

	states: State {
		name: "shown"
		PropertyChanges { toast.y: toast.parent.height - toast.height - 24 - bottomInset; toast.opacity: 1 }
	}
	property real bottomInset: 0

	transitions: Transition {
		NumberAnimation { properties: "y,opacity"; duration: Theme.normal; easing.type: Easing.OutCubic }
	}

	Timer {
		id: hideTimer
		interval: 2800
		onTriggered: toast.state = ""
	}

	Row {
		id: row
		anchors.centerIn: parent
		spacing: 10
		width: Math.min(implicitWidth, toast.width - 36)
		Icon {
			name: toast.kind === "success" ? "check_circle_fill" : (toast.kind === "error" ? "error" : "info")
			color: toast.kind === "success" ? Theme.success : (toast.kind === "error" ? Theme.danger : Theme.info)
			size: 22
			anchors.verticalCenter: parent.verticalCenter
		}
		Text {
			id: label
			width: Math.min(implicitWidth, toast.parent.width - 2 * Theme.pad - 36 - 32)
			wrapMode: Text.Wrap
			color: Theme.dark ? "#1E1B18" : "#F4EFE9"
			font.family: Theme.fontFamily
			font.pixelSize: Theme.fontLabel
			font.weight: Font.Medium
			anchors.verticalCenter: parent.verticalCenter
		}
	}

	MouseArea { anchors.fill: parent; onClicked: toast.state = "" }
}
