import QtQuick
import QtQuick.Layouts

// One computer in the grid: live thumbnail, name, user and status
Item {
	id: card

	required property string uid
	required property string name
	required property string status
	required property string statusText
	required property string user
	required property int screenRevision
	required property bool selected
	required property bool locked

	property bool selectionMode: false

	signal tapped()
	signal longPressed()

	readonly property bool online: status === "online"

	Rectangle {
		id: frame
		anchors.fill: parent
		anchors.margins: 6
		radius: 20
		color: Theme.surface
		border.width: card.selected ? 2.5 : (Theme.dark ? 0 : 1)
		border.color: card.selected ? Theme.accent : Theme.border
		scale: tap.pressed ? 0.97 : 1
		Behavior on scale { NumberAnimation { duration: Theme.fast; easing.type: Easing.OutCubic } }
		Behavior on border.color { ColorAnimation { duration: Theme.fast } }

		Rectangle {
			id: screen
			anchors.left: parent.left
			anchors.right: parent.right
			anchors.top: parent.top
			anchors.margins: 6
			height: width * 10 / 16
			radius: 14
			color: Theme.screenBackground
			clip: true

			Image {
				id: thumbnail
				anchors.fill: parent
				source: card.online && card.screenRevision > 0 ? "image://screen/" + card.uid + "/" + card.screenRevision : ""
				fillMode: Image.PreserveAspectFit
				asynchronous: false
				cache: false
				smooth: true
				mipmap: true
				visible: status === Image.Ready
			}

			// placeholder while there's no picture
			ColumnLayout {
				anchors.centerIn: parent
				width: parent.width - 16
				visible: !thumbnail.visible
				spacing: 6
				Icon {
					Layout.alignment: Qt.AlignHCenter
					name: card.status === "denied" ? "key" : (card.status === "connecting" ? "wifi_find" : "desktop_windows")
					color: card.status === "denied" ? Theme.danger : "#6E6760"
					size: 30
					SequentialAnimation on opacity {
						running: card.status === "connecting"
						loops: Animation.Infinite
						NumberAnimation { from: 1; to: 0.35; duration: 700 }
						NumberAnimation { from: 0.35; to: 1; duration: 700 }
					}
				}
				Text {
					Layout.fillWidth: true
					horizontalAlignment: Text.AlignHCenter
					text: card.statusText
					color: "#A39B93"
					font.family: Theme.fontFamily
					font.pixelSize: 11
					font.weight: Font.Medium
					elide: Text.ElideRight
				}
			}

			// locked overlay
			Rectangle {
				anchors.fill: parent
				visible: card.locked
				color: "#B3000000"
				Rectangle {
					anchors.centerIn: parent
					width: 44; height: 44; radius: 22
					color: Theme.accent
					Icon { anchors.centerIn: parent; name: "lock_fill"; color: "#FFFFFF"; size: 24 }
				}
			}

			// unread chat replies
			Rectangle {
				readonly property int unread: App.chat.unreadTotal >= 0 ? App.chat.unreadCount(card.uid) : 0
				visible: unread > 0
				anchors.right: parent.right
				anchors.top: parent.top
				anchors.margins: 8
				height: 26
				width: unreadRow.implicitWidth + 16
				radius: 13
				color: Theme.accent
				Row {
					id: unreadRow
					anchors.centerIn: parent
					spacing: 4
					Icon { name: "forum_fill"; size: 14; color: "#FFFFFF"; anchors.verticalCenter: parent.verticalCenter }
					Text {
						text: parent.parent.unread
						color: "#FFFFFF"
						font.family: Theme.fontFamily
						font.pixelSize: 12
						font.weight: Font.Bold
						anchors.verticalCenter: parent.verticalCenter
					}
				}
			}

			// status dot
			Rectangle {
				anchors.left: parent.left
				anchors.top: parent.top
				anchors.margins: 8
				width: 10; height: 10; radius: 5
				color: Theme.statusColor(card.status)
				border.width: 2
				border.color: Theme.screenBackground
			}
		}

		ColumnLayout {
			anchors.left: parent.left
			anchors.right: parent.right
			anchors.top: screen.bottom
			anchors.leftMargin: 12
			anchors.rightMargin: 12
			anchors.topMargin: 8
			spacing: 1

			Text {
				Layout.fillWidth: true
				text: card.name
				color: Theme.text
				font.family: Theme.fontFamily
				font.pixelSize: Theme.fontLabel
				font.weight: Font.Bold
				elide: Text.ElideRight
			}
			RowLayout {
				Layout.fillWidth: true
				spacing: 4
				Icon {
					name: card.user.length > 0 ? "person" : ""
					visible: card.user.length > 0
					size: 14
					color: Theme.textMuted
				}
				Text {
					Layout.fillWidth: true
					text: card.user.length > 0 ? card.user : card.statusText
					color: card.user.length > 0 ? Theme.textMuted : Theme.textFaint
					font.family: Theme.fontFamily
					font.pixelSize: Theme.fontCaption
					elide: Text.ElideRight
				}
			}
		}
	}

	// selection check
	Rectangle {
		visible: card.selectionMode
		anchors.right: parent.right
		anchors.top: parent.top
		anchors.margins: 14
		width: 28; height: 28; radius: 14
		color: card.selected ? Theme.accent : "#80000000"
		border.width: 2
		border.color: "#FFFFFF"
		Icon {
			anchors.centerIn: parent
			visible: card.selected
			name: "check"
			size: 18
			color: "#FFFFFF"
		}
	}

	TapHandler {
		id: tap
		onTapped: card.tapped()
		onLongPressed: card.longPressed()
	}
}
