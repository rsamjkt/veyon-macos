import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

// Mode ujian: internet only for the exam websites, chat/game apps closed and
// system shortcuts locked on the target computers (ExamMode plugin) - and
// everything undone with "Akhiri ujian"
Sheet {
	id: sheet

	property var targetUids: []
	property string targetLabel

	function start(uids, label) {
		targetUids = uids
		targetLabel = label
		const settings = App.examSettings()
		sitesField.text = settings.sites
		urlField.text = settings.url
		blockInternet.checked = settings.blockInternet
		closeApps.checked = settings.closeApps
		lockKeys.checked = settings.lockKeys
		open()
	}

	function settings() {
		return {
			sites: sitesField.text,
			url: urlField.text,
			blockInternet: blockInternet.checked,
			closeApps: closeApps.checked,
			lockKeys: lockKeys.checked
		}
	}

	title: qsTr("Mode ujian")
	subtitle: targetLabel
	iconName: "assignment"

	AppText { text: qsTr("Situs ujian (CBT) yang boleh dibuka"); style: "label"; muted: true }
	InputField {
		id: sitesField
		Layout.fillWidth: true
		iconName: "language"
		placeholderText: qsTr("mis. cbt.sekolah.sch.id forms.gle")
		inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
	}

	AppText { Layout.topMargin: 4; text: qsTr("Buka situs ini di komputer (opsional)"); style: "label"; muted: true }
	InputField {
		id: urlField
		Layout.fillWidth: true
		iconName: "open_in_browser"
		placeholderText: "https://cbt.sekolah.sch.id"
		inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
	}

	component OptionRow: RowLayout {
		property alias text: label.text
		property alias checked: toggle.checked
		Layout.fillWidth: true
		spacing: 10
		AppText { id: label; Layout.fillWidth: true }
		Switch { id: toggle }
	}

	OptionRow { id: blockInternet; Layout.topMargin: 4; text: qsTr("Blokir internet selain situs ujian") }
	OptionRow { id: closeApps; text: qsTr("Tutup aplikasi chat, video call, remote & game") }
	OptionRow { id: lockKeys; text: qsTr("Kunci tombol Windows, Alt+Tab, Alt+F4 & Task Manager") }

	RowLayout {
		Layout.fillWidth: true
		Layout.topMargin: 8
		spacing: 10
		AppButton {
			Layout.fillWidth: true
			variant: "outline"
			text: qsTr("Akhiri ujian")
			iconName: "lock_open"
			onClicked: {
				App.endExam(sheet.targetUids)
				sheet.close()
			}
		}
		AppButton {
			Layout.fillWidth: true
			text: qsTr("Mulai ujian")
			iconName: "play_arrow"
			onClicked: {
				if (blockInternet.checked && sitesField.text.trim().length === 0 && urlField.text.trim().length === 0) {
					window.toast(qsTr("Isi situs ujian dulu - tanpa itu komputer tidak bisa internet sama sekali."), "error")
					return
				}
				App.startExam(sheet.settings(), sheet.targetUids)
				sheet.close()
			}
		}
	}

	AppText {
		Layout.fillWidth: true
		text: qsTr("Jaringan kantor tetap bisa diakses. Jika situs ujian memuat file dari alamat lain (mis. fonts.googleapis.com), tambahkan juga. Mode ujian tetap aktif walau komputer dinyalakan ulang.")
		style: "caption"
		muted: true
	}
}
