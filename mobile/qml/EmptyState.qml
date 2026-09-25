import QtQuick
import QtQuick.Layouts

ColumnLayout {
	id: empty

	property string iconName: "desktop_windows"
	property string title
	property string text
	property bool busy: false
	default property alias actions: actionHolder.data

	spacing: 14

	Item {
		Layout.alignment: Qt.AlignHCenter
		width: 112; height: 112

		Rectangle {
			id: pulse
			anchors.centerIn: parent
			width: 112; height: 112; radius: 56
			color: Theme.accentSoft
			SequentialAnimation on scale {
				running: empty.busy && empty.visible
				loops: Animation.Infinite
				NumberAnimation { from: 0.85; to: 1.08; duration: 900; easing.type: Easing.InOutSine }
				NumberAnimation { from: 1.08; to: 0.85; duration: 900; easing.type: Easing.InOutSine }
			}
		}
		Rectangle {
			anchors.centerIn: parent
			width: 76; height: 76; radius: 38
			color: Theme.surface
			Icon { anchors.centerIn: parent; name: empty.iconName; color: Theme.accent; size: 38 }
		}
	}

	AppText {
		Layout.fillWidth: true
		horizontalAlignment: Text.AlignHCenter
		text: empty.title
		style: "heading"
	}
	AppText {
		Layout.fillWidth: true
		horizontalAlignment: Text.AlignHCenter
		text: empty.text
		muted: true
		visible: text.length > 0
	}
	ColumnLayout {
		id: actionHolder
		Layout.alignment: Qt.AlignHCenter
		Layout.topMargin: 6
		spacing: 10
	}
}
