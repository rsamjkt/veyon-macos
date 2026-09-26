import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

// Two-way chat with the user of one computer (Chat plugin)
Page {
	id: page

	property string computerUid
	readonly property var chat: App.chat
	readonly property var messages: chat.computerUid === computerUid ? chat.messages : []
	// the soft keyboard covers the bottom of the edge-to-edge window
	readonly property real keyboardHeight: Qt.inputMethod.visible ? Qt.inputMethod.keyboardRectangle.height : 0

	function send(text) {
		if (text.trim().length === 0)
			return
		if (!chat.send(text)) {
			window.toast(qsTr("%1 sedang tidak terhubung. Pesan belum terkirim.").arg(chat.computerName), "error")
			return
		}
		input.text = ""
	}

	Component.onCompleted: chat.open(computerUid)
	Component.onDestruction: if (chat.computerUid === computerUid) chat.close()

	background: Rectangle { color: Theme.background }

	// ------------------------------------------------------------ header
	header: Rectangle {
		implicitHeight: 64 + window.safeTop
		color: Theme.surface

		Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }

		RowLayout {
			anchors.fill: parent
			anchors.topMargin: window.safeTop
			anchors.leftMargin: 6 + window.safeLeft
			anchors.rightMargin: 8 + window.safeRight
			spacing: 8

			IconButton { iconName: "arrow_back"; onClicked: page.StackView.view.pop() }

			Rectangle {
				width: 42; height: 42; radius: 21
				color: Theme.accentSoft
				Icon { anchors.centerIn: parent; name: "desktop_windows_fill"; color: Theme.accent; size: 22 }
				Rectangle {
					anchors.right: parent.right
					anchors.bottom: parent.bottom
					width: 13; height: 13; radius: 7
					color: page.chat.online ? Theme.success : Theme.textFaint
					border.width: 2
					border.color: Theme.surface
				}
			}

			ColumnLayout {
				Layout.fillWidth: true
				spacing: 0
				AppText {
					Layout.fillWidth: true
					text: page.chat.computerName
					style: "label"
					font.pixelSize: Theme.fontBody
					font.weight: Font.Bold
					elide: Text.ElideRight
					wrapMode: Text.NoWrap
				}
				AppText {
					Layout.fillWidth: true
					text: !page.chat.online ? qsTr("Tidak terhubung")
											: (page.chat.partnerName.length > 0 ? qsTr("Online · %1").arg(page.chat.partnerName)
																				 : qsTr("Online · belum ada yang login"))
					style: "caption"
					color: page.chat.online ? Theme.success : Theme.textFaint
					elide: Text.ElideRight
					wrapMode: Text.NoWrap
				}
			}

			IconButton {
				visible: App.voice.available
				iconName: "mic"
				tooltip: qsTr("Bicara")
				onClicked: window.openVoice(page.computerUid)
			}
		}
	}

	// ------------------------------------------------------------ messages
	ListView {
		id: list
		anchors.top: parent.top
		anchors.left: parent.left
		anchors.right: parent.right
		anchors.bottom: composer.top
		anchors.leftMargin: window.safeLeft
		anchors.rightMargin: window.safeRight
		topMargin: 12
		bottomMargin: 12
		spacing: 4
		clip: true
		model: page.messages
		onCountChanged: Qt.callLater(positionViewAtEnd)
		onHeightChanged: Qt.callLater(positionViewAtEnd)

		delegate: Item {
			id: row
			required property var modelData
			required property int index
			readonly property bool own: modelData.own
			readonly property var previous: index > 0 ? page.messages[index - 1] : null
			readonly property bool firstOfGroup: !previous || previous.own !== own || previous.sender !== modelData.sender

			width: ListView.view.width
			height: bubble.height + (firstOfGroup && index > 0 ? 10 : 0) + (senderLabel.visible ? senderLabel.height + 4 : 0)

			AppText {
				id: senderLabel
				visible: !row.own && row.firstOfGroup
				anchors.left: parent.left
				anchors.leftMargin: Theme.pad + 6
				anchors.bottom: bubble.top
				anchors.bottomMargin: 4
				text: row.modelData.sender
				style: "caption"
				font.weight: Font.Bold
				color: Theme.accent
			}

			Rectangle {
				id: bubble
				readonly property real maxWidth: row.width * 0.78
				anchors.bottom: parent.bottom
				anchors.right: row.own ? parent.right : undefined
				anchors.left: row.own ? undefined : parent.left
				anchors.leftMargin: Theme.pad
				anchors.rightMargin: Theme.pad
				width: Math.min(maxWidth, Math.max(bodyText.implicitWidth, timeText.implicitWidth) + 28)
				height: bodyText.height + timeText.height + 22
				radius: 20
				// flat corner pointing at the sender
				topLeftRadius: !row.own && row.firstOfGroup ? 6 : 20
				topRightRadius: row.own && row.firstOfGroup ? 6 : 20
				color: row.own ? Theme.accent : Theme.surface
				border.width: row.own || Theme.dark ? 0 : 1
				border.color: Theme.border

				Text {
					id: bodyText
					x: 14
					y: 10
					width: Math.min(implicitWidth, bubble.maxWidth - 28)
					text: row.modelData.text
					wrapMode: Text.Wrap
					textFormat: Text.PlainText
					color: row.own ? Theme.textOnAccent : Theme.text
					font.family: Theme.fontFamily
					font.pixelSize: Theme.fontBody
					lineHeight: 1.2
				}
				Text {
					id: timeText
					anchors.right: parent.right
					anchors.rightMargin: 14
					anchors.top: bodyText.bottom
					anchors.topMargin: 2
					text: row.modelData.time
					color: row.own ? "#E6FFFFFF" : Theme.textFaint
					font.family: Theme.fontFamily
					font.pixelSize: 11
				}
			}
		}
	}

	// ------------------------------------------------------------ empty conversation
	ColumnLayout {
		visible: page.messages.length === 0
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.bottom: composer.top
		anchors.bottomMargin: 24
		width: Math.min(parent.width - 48, 420)
		spacing: 16

		EmptyState {
			Layout.fillWidth: true
			iconName: "forum"
			title: qsTr("Mulai percakapan")
			text: qsTr("Pesan Anda muncul di jendela chat pada layar %1. Balasan pengguna tampil di sini.").arg(page.chat.computerName)
		}

		Flow {
			Layout.fillWidth: true
			spacing: 8
			Repeater {
				model: [
					qsTr("Halo, ada yang bisa dibantu?"),
					qsTr("Tolong fokus ke tugasnya, ya."),
					qsTr("Sudah selesai?"),
					qsTr("Mohon simpan pekerjaan sekarang.")
				]
				delegate: Chip {
					required property string modelData
					text: modelData
					enabled: page.chat.online
					onClicked: page.send(modelData)
				}
			}
		}
	}

	// ------------------------------------------------------------ composer
	Rectangle {
		id: composer
		anchors.left: parent.left
		anchors.right: parent.right
		anchors.bottom: parent.bottom
		anchors.bottomMargin: Math.max(0, page.keyboardHeight - window.safeBottom)
		height: composerColumn.implicitHeight + 20 + window.safeBottom
		color: Theme.surface

		Rectangle { width: parent.width; height: 1; color: Theme.border }

		ColumnLayout {
			id: composerColumn
			anchors.left: parent.left
			anchors.right: parent.right
			anchors.top: parent.top
			anchors.topMargin: 10
			anchors.leftMargin: 12 + window.safeLeft
			anchors.rightMargin: 12 + window.safeRight
			spacing: 8

			// offline notice
			RowLayout {
				visible: !page.chat.online
				Layout.fillWidth: true
				spacing: 8
				Icon { name: "wifi_off"; size: 18; color: Theme.warning }
				AppText {
					Layout.fillWidth: true
					text: qsTr("%1 tidak terhubung - pesan baru bisa dikirim setelah komputer online lagi.").arg(page.chat.computerName)
					style: "caption"
					muted: true
				}
			}

			RowLayout {
				Layout.fillWidth: true
				spacing: 8

				InputField {
					id: input
					Layout.fillWidth: true
					placeholderText: qsTr("Tulis pesan…")
					enabled: page.chat.online
					inputMethodHints: Qt.ImhNoPredictiveText
					onAccepted: page.send(text)
				}

				AbstractButton {
					id: sendButton
					implicitWidth: 52; implicitHeight: 52
					enabled: page.chat.online && input.text.trim().length > 0
					background: Rectangle {
						radius: 26
						color: !sendButton.enabled ? Theme.surfaceAlt : (sendButton.pressed ? Theme.accentPressed : Theme.accent)
						Behavior on color { ColorAnimation { duration: Theme.fast } }
					}
					contentItem: Item {
						Icon { anchors.centerIn: parent; name: "send"; size: 22; color: sendButton.enabled ? Theme.textOnAccent : Theme.textFaint }
					}
					scale: pressed ? 0.92 : 1
					Behavior on scale { NumberAnimation { duration: Theme.fast } }
					onClicked: page.send(input.text)
					Accessible.name: qsTr("Kirim")
				}
			}
		}
	}
}
