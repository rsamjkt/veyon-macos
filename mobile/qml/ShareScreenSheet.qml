import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

// Shows the screen of one computer on all the others (Veyon demo)
Sheet {
	id: sheet

	property string sourceUid
	property var targetUids: []

	title: qsTr("Tampilkan layar %1").arg(App.computers.nameOf(sourceUid))
	subtitle: qsTr("Ke semua komputer lain yang tampil")
	iconName: "screen_share"

	Repeater {
		model: [
			{ full: true, icon: "fullscreen", title: qsTr("Layar penuh"), text: qsTr("Semua siswa hanya melihat layar ini; komputer mereka tidak bisa dipakai.") },
			{ full: false, icon: "desktop_windows", title: qsTr("Dalam jendela"), text: qsTr("Tampil sebagai jendela; siswa tetap bisa bekerja.") }
		]
		delegate: ListRow {
			required property var modelData
			Layout.fillWidth: true
			Layout.leftMargin: -Theme.padLarge
			Layout.rightMargin: -Theme.padLarge
			leftPadding: Theme.padLarge
			rightPadding: Theme.padLarge
			iconName: modelData.icon
			title: modelData.title
			subtitle: modelData.text
			onClicked: {
				const problem = App.shareScreen(sheet.sourceUid, modelData.full, sheet.targetUids)
				if (problem.length > 0)
					window.toast(problem, "error")
				sheet.close()
			}
		}
	}

	AppButton {
		Layout.fillWidth: true
		variant: "outline"
		text: qsTr("Hentikan demo yang sedang berjalan")
		iconName: "stop_screen_share"
		onClicked: {
			App.stopDemo([])
			sheet.close()
		}
	}
}
