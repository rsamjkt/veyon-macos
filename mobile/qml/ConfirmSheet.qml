import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

// Confirmation for disruptive actions (shutdown, reboot, logoff)
Sheet {
	id: sheet

	property string action: "off"
	property var targetUids: []
	property string targetLabel: ""
	property int delay: 0

	readonly property var texts: ({
		off: { title: qsTr("Matikan komputer?"), text: qsTr("Pekerjaan yang belum disimpan di komputer bisa hilang."), button: qsTr("Matikan"), icon: "power_settings_new" },
		reboot: { title: qsTr("Mulai ulang komputer?"), text: qsTr("Pekerjaan yang belum disimpan di komputer bisa hilang."), button: qsTr("Mulai ulang"), icon: "restart_alt" },
		logoff: { title: qsTr("Keluarkan pengguna?"), text: qsTr("Pengguna yang sedang login akan di-logout dan aplikasinya ditutup."), button: qsTr("Keluarkan"), icon: "logout" }
	})

	title: texts[action].title
	subtitle: targetLabel
	iconName: texts[action].icon
	iconColor: Theme.danger

	onOpened: delay = 0

	AppText {
		Layout.fillWidth: true
		text: sheet.texts[sheet.action].text
		muted: true
	}

	ColumnLayout {
		visible: sheet.action === "off" && App.hasFeature("PowerDownDelayed")
		Layout.fillWidth: true
		spacing: 8
		AppText { text: qsTr("Waktu"); style: "label"; muted: true }
		RowLayout {
			Layout.fillWidth: true
			spacing: 8
			Repeater {
				model: [ { label: qsTr("Sekarang"), value: 0 }, { label: qsTr("1 menit"), value: 60 }, { label: qsTr("5 menit"), value: 300 }, { label: qsTr("15 menit"), value: 900 } ]
				delegate: Chip {
					required property var modelData
					Layout.fillWidth: true
					text: modelData.label
					selected: sheet.delay === modelData.value
					onClicked: sheet.delay = modelData.value
				}
			}
		}
		AppText {
			visible: sheet.delay > 0
			Layout.fillWidth: true
			text: qsTr("Pengguna akan melihat hitung mundur dan bisa menyimpan pekerjaannya.")
			style: "caption"
			muted: true
		}
	}

	RowLayout {
		Layout.fillWidth: true
		Layout.topMargin: 8
		spacing: 10
		AppButton {
			Layout.fillWidth: true
			variant: "outline"
			text: qsTr("Batal")
			onClicked: sheet.close()
		}
		AppButton {
			Layout.fillWidth: true
			variant: "danger"
			text: sheet.texts[sheet.action].button
			iconName: sheet.texts[sheet.action].icon
			onClicked: {
				if (sheet.action === "off" && sheet.delay > 0)
					App.powerAction("offDelayed", sheet.targetUids, sheet.delay)
				else
					App.powerAction(sheet.action, sheet.targetUids)
				sheet.close()
			}
		}
	}
}
