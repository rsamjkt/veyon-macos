import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtCore

// Asks for a website URL or a program to start on the target computers
Sheet {
	id: sheet

	property string mode: "website"   // "website" or "app"
	property var targetUids: []
	readonly property bool website: mode === "website"

	title: website ? qsTr("Buka website") : qsTr("Jalankan aplikasi")
	subtitle: qsTr("Di %1 komputer").arg(App.targetCount(targetUids))
	iconName: website ? "language" : "rocket_launch"
	iconColor: website ? Theme.info : Theme.accent

	Settings {
		id: history
		category: "history"
		property var websites: []
		property var applications: []
	}

	function remember(value) {
		const key = website ? "websites" : "applications"
		const list = (history[key] || []).filter(v => v !== value)
		list.unshift(value)
		history[key] = list.slice(0, 6)
	}

	onOpened: field.forceActiveFocus()
	onClosed: field.text = ""

	InputField {
		id: field
		Layout.fillWidth: true
		iconName: sheet.website ? "language" : "rocket_launch"
		placeholderText: sheet.website ? qsTr("contoh: google.com") : qsTr("contoh: notepad atau chrome")
		inputMethodHints: sheet.website ? (Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase) : Qt.ImhNoAutoUppercase
		onAccepted: go.clicked()
	}

	AppText {
		visible: !sheet.website
		Layout.fillWidth: true
		text: qsTr("Nama program atau perintah seperti yang diketik di komputer, mis. \"notepad\", \"calc\", atau jalur lengkap ke aplikasi.")
		style: "caption"
		muted: true
	}

	ColumnLayout {
		readonly property var items: (sheet.website ? history.websites : history.applications) || []
		visible: items.length > 0
		Layout.fillWidth: true
		spacing: 8
		AppText { text: qsTr("Terakhir dipakai"); style: "label"; muted: true }
		Flow {
			Layout.fillWidth: true
			spacing: 8
			Repeater {
				model: parent.parent.items
				delegate: Chip {
					required property string modelData
					text: modelData
					iconName: "schedule"
					onClicked: field.text = modelData
				}
			}
		}
	}

	AppButton {
		id: go
		Layout.fillWidth: true
		text: sheet.website ? qsTr("Buka") : qsTr("Jalankan")
		iconName: sheet.website ? "open_in_new" : "rocket_launch"
		enabled: field.text.trim().length > 0
		onClicked: {
			const value = field.text.trim()
			sheet.remember(value)
			if (sheet.website)
				App.openWebsite(value, sheet.targetUids)
			else
				App.startApplication(value, sheet.targetUids)
			sheet.close()
		}
	}
}
