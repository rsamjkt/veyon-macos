import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

// AruniVoice: push-to-talk / open intercom with one computer
Sheet {
	id: sheet

	readonly property var voice: App.voice
	readonly property bool granted: voice.permission === "granted"

	function start(uid) {
		voice.open(uid)
		open()
	}

	title: qsTr("Bicara")
	subtitle: voice.computerName
	iconName: "record_voice_over"
	// holding the talk button must not drag the sheet away
	interactive: !voice.talking

	onOpened: if (!granted) voice.requestPermission()
	onClosed: voice.end()

	Connections {
		target: sheet.voice
		function onProblem(message) { window.toast(message, "error") }
	}

	// ------------------------------------------------------------ status
	RowLayout {
		Layout.fillWidth: true
		spacing: 8
		Rectangle { width: 8; height: 8; radius: 4; color: sheet.voice.online ? Theme.success : Theme.textFaint }
		AppText {
			Layout.fillWidth: true
			text: !sheet.voice.online ? qsTr("%1 tidak terhubung").arg(sheet.voice.computerName)
									  : (sheet.voice.intercom ? qsTr("Interkom aktif - dua arah")
															  : qsTr("Siap - tahan tombol untuk bicara"))
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
						text: sheet.voice.permission === "denied"
							  ? qsTr("Akses mikrofon ditolak. Buka pengaturan aplikasi → Izin → Mikrofon, lalu pilih Izinkan.")
							  : qsTr("AruniControl perlu mikrofon HP agar suara Anda terdengar di komputer.")
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
					onClicked: sheet.voice.requestPermission()
				}
				AppButton {
					visible: sheet.voice.permission === "denied" && App.isMobile
					Layout.fillWidth: true
					compact: true
					variant: "outline"
					text: qsTr("Buka pengaturan")
					iconName: "settings"
					onClicked: sheet.voice.openPermissionSettings()
				}
			}
		}
	}

	// ------------------------------------------------------------ talk button
	Item {
		Layout.fillWidth: true
		Layout.preferredHeight: 230

		// sound rings: own voice (accent) and the computer's (info)
		Rectangle {
			anchors.centerIn: talkButton
			width: talkButton.width * (1 + 0.55 * sheet.voice.micLevel)
			height: width
			radius: width / 2
			color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.18)
			visible: sheet.voice.talking
			Behavior on width { NumberAnimation { duration: 90 } }
		}
		Rectangle {
			anchors.centerIn: talkButton
			width: talkButton.width * (1 + 0.55 * sheet.voice.remoteLevel)
			height: width
			radius: width / 2
			color: "transparent"
			border.width: 4
			border.color: Qt.rgba(Theme.info.r, Theme.info.g, Theme.info.b, 0.55)
			visible: sheet.voice.remoteSpeaking
			Behavior on width { NumberAnimation { duration: 90 } }
		}

		AbstractButton {
			id: talkButton
			anchors.centerIn: parent
			width: 148; height: 148
			enabled: sheet.voice.online && sheet.granted && !sheet.voice.intercom
			background: Rectangle {
				radius: width / 2
				gradient: Gradient {
					GradientStop { position: 0; color: talkButton.enabled || sheet.voice.intercom ? Theme.gradientTop : Theme.surfaceHigh }
					GradientStop { position: 1; color: talkButton.enabled || sheet.voice.intercom ? Theme.gradientBottom : Theme.surfaceHigh }
				}
			}
			contentItem: Item {
				ColumnLayout {
					anchors.centerIn: parent
					spacing: 6
					Icon {
						Layout.alignment: Qt.AlignHCenter
						name: sheet.voice.talking ? "mic_fill" : "mic"
						size: 48
						color: talkButton.enabled || sheet.voice.intercom ? "#FFFFFF" : Theme.textFaint
					}
					Text {
						Layout.alignment: Qt.AlignHCenter
						text: sheet.voice.intercom ? qsTr("Interkom") : (sheet.voice.talking ? qsTr("Bicara…") : qsTr("Tahan"))
						color: talkButton.enabled || sheet.voice.intercom ? "#FFFFFF" : Theme.textFaint
						font.family: Theme.fontFamily
						font.pixelSize: Theme.fontLabel
						font.weight: Font.Bold
					}
				}
			}
			scale: sheet.voice.talking && !sheet.voice.intercom ? 1.06 : (pressed ? 0.96 : 1)
			Behavior on scale { NumberAnimation { duration: Theme.fast; easing.type: Easing.OutCubic } }
			onPressed: sheet.voice.startTalking()
			onReleased: sheet.voice.stopTalking()
			onCanceled: sheet.voice.stopTalking()
			Accessible.name: qsTr("Tahan untuk bicara")
		}
	}

	// ------------------------------------------------------------ level meters
	Repeater {
		model: [
			{ label: qsTr("Suara Anda"), icon: "mic", remote: false },
			{ label: qsTr("Suara dari komputer"), icon: "graphic_eq", remote: true }
		]
		delegate: RowLayout {
			id: meter
			required property var modelData
			readonly property real level: modelData.remote ? sheet.voice.remoteLevel : sheet.voice.micLevel
			readonly property color tint: modelData.remote ? Theme.info : Theme.accent
			Layout.fillWidth: true
			spacing: 10
			Icon { name: meter.modelData.icon; size: 20; color: meter.tint }
			AppText { text: meter.modelData.label; style: "caption"; muted: true; Layout.preferredWidth: 130; wrapMode: Text.NoWrap }
			Rectangle {
				Layout.fillWidth: true
				height: 8; radius: 4
				color: Theme.surfaceAlt
				Rectangle {
					width: parent.width * meter.level
					height: parent.height
					radius: 4
					color: meter.tint
					Behavior on width { NumberAnimation { duration: 90 } }
				}
			}
		}
	}

	// ------------------------------------------------------------ options
	RowLayout {
		Layout.fillWidth: true
		Layout.topMargin: 4
		spacing: 8
		Chip {
			Layout.fillWidth: true
			text: qsTr("Interkom dua arah")
			iconName: "headset_mic"
			selected: sheet.voice.intercom
			enabled: sheet.voice.online && sheet.granted
			onClicked: sheet.voice.setIntercom(!sheet.voice.intercom)
		}
		Chip {
			Layout.fillWidth: true
			text: sheet.voice.speakerMuted ? qsTr("Speaker mati") : qsTr("Speaker nyala")
			iconName: sheet.voice.speakerMuted ? "volume_off" : "volume_up"
			selected: sheet.voice.speakerMuted
			onClicked: sheet.voice.speakerMuted = !sheet.voice.speakerMuted
		}
	}

	AppText {
		Layout.fillWidth: true
		text: sheet.voice.intercom
			  ? qsTr("Mikrofon HP dan komputer terbuka terus. Pakai headset agar tidak berdengung.")
			  : qsTr("Pengguna komputer membalas dengan menahan tombol di jendela AruniVoice di layarnya.")
		style: "caption"
		muted: true
	}

	AppButton {
		Layout.fillWidth: true
		Layout.topMargin: 4
		variant: "danger"
		text: qsTr("Akhiri")
		iconName: "call_end"
		onClicked: sheet.close()
	}
}
