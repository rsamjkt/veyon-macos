import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

// Siaran suara: one-way announcement to all target computers (hold-to-talk
// or an open microphone). The computers only play the audio and show a small
// notice - nothing comes back.
Sheet {
	id: sheet

	readonly property var broadcast: App.broadcast
	readonly property bool granted: broadcast.permission === "granted"

	function start(uids, label) {
		broadcast.open(uids, label)
		open()
	}

	title: qsTr("Siaran suara")
	subtitle: broadcast.targetLabel
	iconName: "campaign"
	// holding the talk button must not drag the sheet away
	interactive: !broadcast.talking

	onOpened: if (!granted) broadcast.requestPermission()
	onClosed: broadcast.end()

	Connections {
		target: sheet.broadcast
		function onProblem(message) { window.toast(message, "error") }
	}

	// ------------------------------------------------------------ status
	RowLayout {
		Layout.fillWidth: true
		spacing: 8
		Rectangle { width: 8; height: 8; radius: 4; color: sheet.broadcast.onlineCount > 0 ? Theme.success : Theme.textFaint }
		AppText {
			Layout.fillWidth: true
			text: sheet.broadcast.onlineCount === 0
				  ? qsTr("Tidak ada komputer tujuan yang terhubung")
				  : (sheet.broadcast.talking
					 ? qsTr("Menyiarkan ke %1 komputer…").arg(sheet.broadcast.onlineCount)
					 : qsTr("%1 dari %2 komputer terhubung - tahan tombol untuk bicara")
					   .arg(sheet.broadcast.onlineCount).arg(sheet.broadcast.targetCount))
			style: "label"
			muted: true
			font.weight: Font.Medium
		}
	}

	// ------------------------------------------------------------ microphone permission
	Rectangle {
		visible: !sheet.granted
		Layout.fillWidth: true
		implicitHeight: permissionColumn.implicitHeight + 28
		radius: Theme.radius
		color: Theme.accentSoft

		ColumnLayout {
			id: permissionColumn
			anchors.fill: parent
			anchors.margins: 14
			spacing: 10
			RowLayout {
				Layout.fillWidth: true
				spacing: 12
				Icon { name: "mic_off"; color: Theme.accent; size: 26 }
				ColumnLayout {
					Layout.fillWidth: true
					spacing: 2
					AppText { Layout.fillWidth: true; text: qsTr("Izinkan mikrofon"); style: "label" }
					AppText {
						Layout.fillWidth: true
						text: sheet.broadcast.permission === "denied"
							  ? qsTr("Akses mikrofon ditolak. Buka pengaturan aplikasi → Izin → Mikrofon, lalu pilih Izinkan.")
							  : qsTr("AruniControl perlu mikrofon HP agar pengumuman Anda terdengar di semua komputer.")
						style: "caption"
						muted: true
					}
				}
			}
			RowLayout {
				Layout.fillWidth: true
				spacing: 8
				AppButton {
					Layout.fillWidth: true
					compact: true
					text: qsTr("Izinkan")
					iconName: "mic"
					onClicked: sheet.broadcast.requestPermission()
				}
				AppButton {
					visible: sheet.broadcast.permission === "denied" && App.isMobile
					Layout.fillWidth: true
					compact: true
					variant: "outline"
					text: qsTr("Buka pengaturan")
					iconName: "settings"
					onClicked: App.voice.openPermissionSettings()
				}
			}
		}
	}

	// ------------------------------------------------------------ talk button
	Item {
		Layout.fillWidth: true
		Layout.preferredHeight: 240

		// own voice level
		Rectangle {
			anchors.centerIn: talkButton
			width: talkButton.width * (1 + 0.55 * sheet.broadcast.micLevel)
			height: width
			radius: width / 2
			color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
			visible: sheet.broadcast.talking
			Behavior on width { NumberAnimation { duration: 90 } }
		}

		AbstractButton {
			id: talkButton
			readonly property bool lit: enabled || sheet.broadcast.keepOpen
			anchors.centerIn: parent
			width: 160; height: 160
			enabled: sheet.broadcast.onlineCount > 0 && sheet.granted && !sheet.broadcast.keepOpen
			background: Rectangle {
				radius: width / 2
				gradient: Gradient {
					GradientStop { position: 0; color: talkButton.lit ? Theme.gradientTop : Theme.surfaceHigh }
					GradientStop { position: 1; color: talkButton.lit ? Theme.gradientBottom : Theme.surfaceHigh }
				}
			}
			contentItem: Item {
				ColumnLayout {
					anchors.centerIn: parent
					spacing: 6
					Icon {
						Layout.alignment: Qt.AlignHCenter
						name: sheet.broadcast.talking ? "campaign" : "mic"
						size: 52
						color: talkButton.lit ? "#FFFFFF" : Theme.textFaint
					}
					Text {
						Layout.alignment: Qt.AlignHCenter
						text: sheet.broadcast.keepOpen ? qsTr("Mikrofon terbuka")
													   : (sheet.broadcast.talking ? qsTr("Menyiarkan…") : qsTr("Tahan"))
						color: talkButton.lit ? "#FFFFFF" : Theme.textFaint
						font.family: Theme.fontFamily
						font.pixelSize: Theme.fontLabel
						font.weight: Font.Bold
					}
				}
			}
			scale: sheet.broadcast.talking && !sheet.broadcast.keepOpen ? 1.06 : (pressed ? 0.96 : 1)
			Behavior on scale { NumberAnimation { duration: Theme.fast; easing.type: Easing.OutCubic } }
			onPressed: sheet.broadcast.startTalking()
			onReleased: sheet.broadcast.stopTalking()
			onCanceled: sheet.broadcast.stopTalking()
			Accessible.name: qsTr("Tahan untuk menyiarkan")
		}
	}

	// ------------------------------------------------------------ level meter
	RowLayout {
		Layout.fillWidth: true
		spacing: 10
		Icon { name: "mic"; size: 20; color: Theme.accent }
		AppText { text: qsTr("Suara Anda"); style: "caption"; muted: true; Layout.preferredWidth: 90; wrapMode: Text.NoWrap }
		Rectangle {
			Layout.fillWidth: true
			height: 8; radius: 4
			color: Theme.surfaceAlt
			Rectangle {
				width: parent.width * sheet.broadcast.micLevel
				height: parent.height
				radius: 4
				color: Theme.accent
				Behavior on width { NumberAnimation { duration: 90 } }
			}
		}
	}

	// ------------------------------------------------------------ options
	Chip {
		Layout.fillWidth: true
		Layout.topMargin: 4
		text: qsTr("Biarkan mikrofon terbuka")
		iconName: "mic_fill"
		selected: sheet.broadcast.keepOpen
		enabled: sheet.broadcast.onlineCount > 0 && sheet.granted
		onClicked: sheet.broadcast.setKeepOpen(!sheet.broadcast.keepOpen)
	}

	AppText {
		Layout.fillWidth: true
		text: sheet.broadcast.keepOpen
			  ? qsTr("Semua yang Anda ucapkan terdengar di komputer sampai mikrofon ditutup.")
			  : qsTr("Suara hanya satu arah. Komputer menampilkan tanda kecil \"sedang berbicara\" selama siaran, tanpa mengganggu pekerjaan siswa.")
		style: "caption"
		muted: true
	}

	AppButton {
		Layout.fillWidth: true
		Layout.topMargin: 4
		variant: "danger"
		text: qsTr("Selesai")
		iconName: "call_end"
		onClicked: sheet.close()
	}
}
