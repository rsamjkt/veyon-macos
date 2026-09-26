import QtQuick

// Small red counter (unread chat replies)
Rectangle {
	id: badge

	property int count: 0
	property color ringColor: Theme.surface

	visible: count > 0
	height: 20
	width: Math.max(height, label.implicitWidth + 10)
	radius: height / 2
	color: Theme.danger
	border.width: 2
	border.color: ringColor

	Text {
		id: label
		anchors.centerIn: parent
		text: badge.count > 99 ? "99+" : badge.count
		color: "#FFFFFF"
		font.family: Theme.fontFamily
		font.pixelSize: 11
		font.weight: Font.Bold
	}
}
