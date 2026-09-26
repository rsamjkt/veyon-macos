import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

// Blokir situs: block a list of websites on the target computers (SiteFilter
// plugin) and show each computer's answer
Sheet {
	id: sheet

	readonly property var filter: App.siteFilter
	property var sites: []
	// the list was changed by the user - don't overwrite it with query results
	property bool edited: false

	function start(uids, label) {
		sites = []
		edited = false
		domainField.text = ""
		filter.open(uids, label)
		open()
	}

	function hasAll(list) {
		return list.every(s => sites.indexOf(s) >= 0)
	}

	function togglePreset(list) {
		edited = true
		if (hasAll(list))
			sites = sites.filter(s => list.indexOf(s) < 0)
		else
			sites = sites.concat(list.filter(s => sites.indexOf(s) < 0))
	}

	function addDomain() {
		const entries = domainField.text.split(/[\s,;]+/).filter(t => t.length > 0)
		if (entries.length === 0)
			return
		const added = []
		const invalid = []
		for (const entry of entries) {
			const domain = filter.normalizedDomain(entry)
			if (domain.length === 0)
				invalid.push(entry)
			else if (sites.indexOf(domain) < 0 && added.indexOf(domain) < 0)
				added.push(domain)
		}
		if (added.length > 0) {
			edited = true
			sites = sites.concat(added)
		}
		domainField.text = invalid.join(" ")
		if (invalid.length > 0)
			window.toast(qsTr("Bukan alamat situs: %1").arg(invalid.join(", ")), "error")
	}

	function stateText(result) {
		switch (result.state) {
		case "pending":
			return filter.lastAction === "query" ? qsTr("Memeriksa…") : qsTr("Menunggu jawaban…")
		case "offline": return qsTr("Tidak terhubung")
		case "noresponse": return qsTr("Tidak menjawab (AruniControl versi lama?)")
		case "unsupported": return result.error.length > 0 ? result.error : qsTr("Tidak didukung di komputer ini")
		case "error": return result.error
		}
		return result.sites.length > 0 ? qsTr("%1 situs diblokir").arg(result.sites.length) : qsTr("Tidak ada situs diblokir")
	}

	function stateColor(state) {
		switch (state) {
		case "ok": return Theme.success
		case "error":
		case "unsupported": return Theme.danger
		case "noresponse": return Theme.warning
		}
		return Theme.textMuted
	}

	title: qsTr("Blokir situs")
	subtitle: filter.targetLabel
	iconName: "block"

	onClosed: filter.close()

	// prefill with what the computers block right now
	Connections {
		target: sheet.filter
		function onResultsChanged() {
			if (!sheet.edited && sheet.filter.lastAction === "query")
				sheet.sites = sheet.filter.reportedSites
		}
	}

	// ------------------------------------------------------------ presets
	AppText { text: qsTr("Daftar siap pakai"); style: "label"; muted: true }
	Flow {
		Layout.fillWidth: true
		spacing: 8
		Repeater {
			model: sheet.filter.presets
			delegate: Chip {
				required property var modelData
				text: modelData.name
				iconName: sheet.hasAll(modelData.sites) ? "check" : modelData.icon
				selected: sheet.hasAll(modelData.sites)
				onClicked: sheet.togglePreset(modelData.sites)
			}
		}
	}

	// ------------------------------------------------------------ own domains
	AppText { Layout.topMargin: 4; text: qsTr("Tambah situs"); style: "label"; muted: true }
	RowLayout {
		Layout.fillWidth: true
		spacing: 8
		InputField {
			id: domainField
			Layout.fillWidth: true
			iconName: "language"
			placeholderText: qsTr("mis. youtube.com")
			inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
			onAccepted: sheet.addDomain()
		}
		AppButton {
			variant: "tonal"
			iconName: "add"
			text: qsTr("Tambah")
			enabled: domainField.text.trim().length > 0
			onClicked: sheet.addDomain()
		}
	}

	// ------------------------------------------------------------ list
	AppText {
		Layout.topMargin: 4
		text: sheet.sites.length > 0 ? qsTr("%1 situs akan diblokir").arg(sheet.sites.length) : qsTr("Belum ada situs dipilih")
		style: "label"
		muted: true
	}
	Flow {
		Layout.fillWidth: true
		spacing: 8
		visible: sheet.sites.length > 0
		Repeater {
			model: sheet.sites
			delegate: Chip {
				required property string modelData
				text: modelData
				iconName: "close"
				Accessible.name: qsTr("Hapus %1").arg(modelData)
				onClicked: {
					sheet.edited = true
					sheet.sites = sheet.sites.filter(s => s !== modelData)
				}
			}
		}
	}

	RowLayout {
		Layout.fillWidth: true
		Layout.topMargin: 8
		spacing: 10
		AppButton {
			Layout.fillWidth: true
			variant: "outline"
			text: qsTr("Buka blokir")
			iconName: "public"
			onClicked: {
				if (sheet.filter.unblock() === 0)
					window.toast(qsTr("Tidak ada komputer tujuan yang terhubung."), "error")
				else {
					sheet.edited = true
					sheet.sites = []
				}
			}
		}
		AppButton {
			Layout.fillWidth: true
			text: qsTr("Blokir")
			iconName: "block"
			enabled: sheet.sites.length > 0
			onClicked: {
				if (sheet.filter.block(sheet.sites) === 0)
					window.toast(qsTr("Tidak ada komputer tujuan yang terhubung."), "error")
			}
		}
	}

	AppText {
		Layout.fillWidth: true
		text: qsTr("Berlaku untuk semua browser di komputer. Di Mac, pengguna komputer diminta memasukkan kata sandi admin.")
		style: "caption"
		muted: true
	}

	// ------------------------------------------------------------ answers per computer
	AppText {
		visible: sheet.filter.results.length > 0
		Layout.topMargin: 8
		text: qsTr("Status per komputer")
		style: "label"
		muted: true
	}
	Repeater {
		model: sheet.filter.results
		delegate: RowLayout {
			id: resultRow
			required property var modelData
			Layout.fillWidth: true
			spacing: 12
			Rectangle {
				width: 36; height: 36; radius: 12
				color: Theme.surfaceAlt
				Icon {
					anchors.centerIn: parent
					name: resultRow.modelData.state === "ok" ? "check_circle" :
						  (resultRow.modelData.state === "offline" ? "wifi_off" :
						  (resultRow.modelData.state === "pending" ? "schedule" : "error"))
					color: sheet.stateColor(resultRow.modelData.state)
					size: 20
				}
			}
			ColumnLayout {
				Layout.fillWidth: true
				spacing: 1
				AppText { Layout.fillWidth: true; text: resultRow.modelData.name; style: "label"; elide: Text.ElideRight; wrapMode: Text.NoWrap }
				AppText {
					Layout.fillWidth: true
					text: sheet.stateText(resultRow.modelData)
					style: "caption"
					color: sheet.stateColor(resultRow.modelData.state)
				}
			}
			BusyIndicator {
				visible: resultRow.modelData.state === "pending"
				running: visible
				implicitWidth: 24
				implicitHeight: 24
			}
		}
	}
}
