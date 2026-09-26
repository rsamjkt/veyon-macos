import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs

// All actions for the current target computers as a grid of tiles
Sheet {
	id: sheet

	property var targetUids: []
	property string targetLabel: ""

	signal messageRequested(var uids, string label)

	title: qsTr("Aksi")
	subtitle: targetLabel
	iconName: "bolt_fill"

	readonly property var groups: [
		{
			title: qsTr("Kelas"),
			actions: [
				{ key: "lock", icon: "lock", label: qsTr("Kunci layar"), feature: "ScreenLock" },
				{ key: "unlock", icon: "lock_open", label: qsTr("Buka kunci"), feature: "ScreenLock" },
				{ key: "message", icon: "chat", label: qsTr("Kirim pesan"), feature: "TextMessage" },
				{ key: "broadcast", icon: "campaign", label: qsTr("Siaran suara"), feature: "AruniVoiceBroadcast" },
				{ key: "screenshot", icon: "photo_camera", label: qsTr("Tangkap layar"), feature: "" },
				{ key: "website", icon: "language", label: qsTr("Buka website"), feature: "OpenWebsite" },
				{ key: "app", icon: "rocket_launch", label: qsTr("Jalankan aplikasi"), feature: "StartApp" },
				{ key: "inputLock", icon: "keyboard_off", label: qsTr("Kunci keyboard & mouse"), feature: "InputDevicesLock" },
				{ key: "inputUnlock", icon: "keyboard", label: qsTr("Buka keyboard & mouse"), feature: "InputDevicesLock" }
			]
		},
		{
			title: qsTr("Komunikasi & pantau (satu komputer)"),
			actions: [
				{ key: "chat", icon: "forum", label: qsTr("Chat"), feature: "Chat" },
				{ key: "voice", icon: "record_voice_over", label: qsTr("Bicara"), feature: "AruniVoice" },
				{ key: "apps", icon: "apps", label: qsTr("Aplikasi berjalan"), feature: "ApplicationMonitoring" },
				{ key: "accessLog", icon: "history", label: qsTr("Log akses"), feature: "" }
			]
		},
		{
			title: qsTr("Demo & file"),
			actions: [
				{ key: "share", icon: "screen_share", label: qsTr("Tampilkan layar siswa"), feature: "ShareUserScreenFullScreen" },
				{ key: "stopDemo", icon: "stop_screen_share", label: qsTr("Hentikan demo"), feature: "Demo" },
				{ key: "sendFiles", icon: "upload_file", label: qsTr("Kirim file"), feature: "DistributeFiles" },
				{ key: "collect", icon: "download", label: qsTr("Kumpulkan file"), feature: "FileCollect" }
			]
		},
		{
			title: qsTr("Pembatasan"),
			actions: [
				{ key: "internetOff", icon: "public_off", label: qsTr("Blokir internet"), feature: "InternetAccessControl" },
				{ key: "internetOn", icon: "public", label: qsTr("Buka internet"), feature: "InternetAccessControl" },
				{ key: "siteBlock", icon: "block", label: qsTr("Blokir situs"), feature: "" },
				{ key: "mute", icon: "volume_off", label: qsTr("Bisukan suara"), feature: "AruniMediaMute" },
				{ key: "unmute", icon: "volume_up", label: qsTr("Nyalakan suara"), feature: "AruniMediaMute" }
			]
		},
		{
			title: qsTr("Daya & sesi"),
			actions: [
				{ key: "on", icon: "power", label: qsTr("Nyalakan"), feature: "PowerOn" },
				{ key: "login", icon: "login", label: qsTr("Login pengguna"), feature: "UserLogin" },
				{ key: "reboot", icon: "restart_alt", label: qsTr("Mulai ulang"), feature: "Reboot", danger: true },
				{ key: "off", icon: "power_settings_new", label: qsTr("Matikan"), feature: "PowerDownNow", danger: true },
				{ key: "logoff", icon: "logout", label: qsTr("Keluarkan pengguna"), feature: "UserLogoff", danger: true }
			]
		}
	]

	function run(key) {
		const uids = targetUids
		switch (key) {
		case "lock": App.lockScreens(true, uids); break
		case "unlock": App.lockScreens(false, uids); break
		case "message":
			sheet.close()
			sheet.messageRequested(uids, targetLabel)
			return
		case "screenshot": App.saveScreenshots(uids); break
		case "website":
			inputSheet.mode = "website"
			inputSheet.targetUids = uids
			inputSheet.open()
			break
		case "app":
			inputSheet.mode = "app"
			inputSheet.targetUids = uids
			inputSheet.open()
			break
		case "inputLock": App.lockInput(true, uids); break
		case "inputUnlock": App.lockInput(false, uids); break
		case "share":
			if (uids.length !== 1) {
				window.toast(qsTr("Tahan kartu satu komputer untuk memilih layar yang akan ditampilkan"), "info")
				break
			}
			shareSheet.sourceUid = uids[0]
			shareSheet.open()
			break
		case "broadcast":
			sheet.close()
			window.openBroadcast(uids, targetLabel)
			return
		case "siteBlock":
			sheet.close()
			window.openSiteBlock(uids, targetLabel)
			return
		case "chat":
		case "voice":
		case "apps":
		case "accessLog":
			if (uids.length !== 1) {
				window.toast(qsTr("Tahan kartu satu komputer untuk memilihnya dulu"), "info")
				break
			}
			sheet.close()
			if (key === "chat")
				window.openChat(uids[0])
			else if (key === "voice")
				window.openVoice(uids[0])
			else if (key === "accessLog")
				window.openAccessLog(uids[0])
			else
				window.openApps(uids[0])
			return
		case "stopDemo": App.stopDemo([]); break
		case "sendFiles":
			filesDialog.targetUids = uids
			filesDialog.open()
			break
		case "collect": App.collectFiles(uids); break
		case "login":
			loginSheet.targetUids = uids
			loginSheet.open()
			break
		case "internetOff": App.setInternetBlocked(true, uids); break
		case "internetOn": App.setInternetBlocked(false, uids); break
		case "mute": App.setAudioMuted(true, uids); break
		case "unmute": App.setAudioMuted(false, uids); break
		case "on": App.powerAction("on", uids); break
		case "reboot":
		case "off":
		case "logoff":
			confirmSheet.action = key
			confirmSheet.targetUids = uids
			confirmSheet.targetLabel = targetLabel
			confirmSheet.open()
			break
		}
		sheet.close()
	}

	Repeater {
		model: sheet.groups
		delegate: ColumnLayout {
			id: group
			required property var modelData
			readonly property var available: modelData.actions.filter(a => a.feature.length === 0 || App.hasFeature(a.feature))
			Layout.fillWidth: true
			visible: available.length > 0
			spacing: 10

			AppText { text: group.modelData.title; style: "label"; muted: true }

			GridLayout {
				Layout.fillWidth: true
				columns: Math.max(3, Math.floor(width / 110))
				uniformCellWidths: true
				columnSpacing: 10
				rowSpacing: 10

				Repeater {
					model: group.available
					delegate: AbstractButton {
						id: tile
						required property var modelData
						readonly property color tint: modelData.danger ? Theme.danger : Theme.accent
						Layout.fillWidth: true
						Layout.minimumWidth: 0
						Layout.preferredWidth: 100
						Layout.preferredHeight: 96
						background: Rectangle {
							radius: 20
							color: tile.pressed ? Theme.surfaceHigh : Theme.surfaceAlt
							Behavior on color { ColorAnimation { duration: Theme.fast } }
						}
						contentItem: ColumnLayout {
							spacing: 8
							Rectangle {
								Layout.alignment: Qt.AlignHCenter
								width: 42; height: 42; radius: 14
								color: Qt.rgba(tile.tint.r, tile.tint.g, tile.tint.b, Theme.dark ? 0.22 : 0.13)
								Icon { anchors.centerIn: parent; name: tile.modelData.icon; color: tile.tint; size: 24 }
							}
							Text {
								Layout.fillWidth: true
								horizontalAlignment: Text.AlignHCenter
								text: tile.modelData.label
								color: Theme.text
								font.family: Theme.fontFamily
								font.pixelSize: 12
								font.weight: Font.DemiBold
								wrapMode: Text.Wrap
								maximumLineCount: 2
							}
						}
						scale: pressed ? 0.95 : 1
						Behavior on scale { NumberAnimation { duration: Theme.fast } }
						onClicked: sheet.run(modelData.key)
					}
				}
			}
		}
	}

	TextInputSheet { id: inputSheet }
	ConfirmSheet { id: confirmSheet }
	ShareScreenSheet { id: shareSheet }
	LoginSheet { id: loginSheet }

	FileDialog {
		id: filesDialog
		property var targetUids: []
		title: qsTr("Pilih file untuk dikirim")
		fileMode: FileDialog.OpenFiles
		onAccepted: {
			const problem = App.sendFiles(selectedFiles, targetUids)
			if (problem.length > 0)
				window.toast(problem, "error")
		}
	}
}
