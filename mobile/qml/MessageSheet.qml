import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Sheet {
	id: sheet

	property var targetUids: []
	property string targetLabel: ""

	title: qsTr("Kirim pesan")
	subtitle: targetLabel
	iconName: "chat"
	iconColor: Theme.info

	onOpened: messageField.forceActiveFocus()
	onClosed: {
		messageField.text = ""
		titleField.text = ""
	}

	AppText { text: qsTr("Pesan cepat"); style: "label"; muted: true }

	Flow {
		Layout.fillWidth: true
		spacing: 8
		Repeater {
			model: [
				qsTr("Perhatikan ke depan, ya 🙏"),
				qsTr("Waktu tinggal 5 menit. Simpan pekerjaan kalian."),
				qsTr("Istirahat 10 menit."),
				qsTr("Silakan logout dan matikan komputer."),
				qsTr("Kumpulkan tugas sekarang.")
			]
			delegate: Chip {
				required property string modelData
				text: modelData
				onClicked: messageField.text = modelData
			}
		}
	}

	InputField {
		id: titleField
		Layout.fillWidth: true
		Layout.topMargin: 4
		placeholderText: qsTr("Judul (opsional)")
		iconName: "campaign"
	}

	TextArea {
		id: messageField
		Layout.fillWidth: true
		Layout.preferredHeight: Math.max(120, implicitHeight)
		placeholderText: qsTr("Tulis pesan untuk ditampilkan di layar…")
		wrapMode: TextEdit.Wrap
		font.family: Theme.fontFamily
		font.pixelSize: Theme.fontBody
		color: Theme.text
		placeholderTextColor: Theme.textFaint
		padding: 16
		background: Rectangle {
			radius: Theme.radius
			color: Theme.surfaceAlt
			border.width: messageField.activeFocus ? 2 : 0
			border.color: Theme.accent
		}
	}

	AppButton {
		Layout.fillWidth: true
		Layout.topMargin: 4
		text: qsTr("Kirim ke %1").arg(App.targetCount(sheet.targetUids)) + " " + qsTr("komputer")
		iconName: "send"
		enabled: messageField.text.trim().length > 0
		onClicked: {
			App.sendMessage(titleField.text.trim(), messageField.text.trim(), sheet.targetUids)
			sheet.close()
		}
	}
}
