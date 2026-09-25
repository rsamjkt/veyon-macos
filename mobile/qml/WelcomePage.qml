import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Page {
	id: page

	background: Rectangle { color: Theme.background }

	// sunrise hero
	Rectangle {
		id: hero
		anchors.left: parent.left
		anchors.right: parent.right
		anchors.top: parent.top
		height: parent.height * 0.46 + Theme.radiusSheet
		clip: true
		gradient: Gradient {
			GradientStop { position: 0; color: Theme.gradientTop }
			GradientStop { position: 1; color: Theme.gradientBottom }
		}

		// soft sun glow
		Rectangle {
			width: parent.width * 1.1; height: width; radius: width / 2
			anchors.horizontalCenter: parent.horizontalCenter
			y: parent.height * 0.55
			color: "#FFFFFF"
			opacity: 0.12
		}
		Rectangle {
			width: parent.width * 0.7; height: width; radius: width / 2
			anchors.horizontalCenter: parent.horizontalCenter
			y: parent.height * 0.72
			color: "#FFFFFF"
			opacity: 0.12
		}

		Image {
			id: logo
			source: Qt.resolvedUrl("images/logo.svg")
			sourceSize: Qt.size(112, 112)
			width: 112; height: 112
			anchors.centerIn: parent
			anchors.verticalCenterOffset: window.safeTop / 2 - Theme.radiusSheet / 2

			SequentialAnimation on anchors.verticalCenterOffset {
				running: page.visible
				loops: Animation.Infinite
				NumberAnimation { to: window.safeTop / 2 - 6; duration: 1800; easing.type: Easing.InOutSine }
				NumberAnimation { to: window.safeTop / 2 + 6; duration: 1800; easing.type: Easing.InOutSine }
			}
		}

	}

	// content panel overlapping the hero with rounded top corners
	Rectangle {
		anchors.fill: column
		anchors.topMargin: -Theme.padLarge
		anchors.leftMargin: -Theme.padLarge
		anchors.rightMargin: -Theme.padLarge
		anchors.bottomMargin: -(Theme.padLarge + window.safeBottom + Theme.radiusSheet)
		radius: Theme.radiusSheet
		color: Theme.background
	}

	ColumnLayout {
		id: column
		anchors.top: hero.bottom
		anchors.topMargin: -Theme.radiusSheet + Theme.padLarge
		anchors.left: parent.left
		anchors.right: parent.right
		anchors.bottom: parent.bottom
		anchors.leftMargin: Theme.padLarge
		anchors.rightMargin: Theme.padLarge
		anchors.bottomMargin: Theme.padLarge + window.safeBottom
		spacing: 0

		AppText {
			Layout.fillWidth: true
			text: "AruniControl"
			style: "display"
		}
		AppText {
			Layout.fillWidth: true
			Layout.topMargin: 6
			text: qsTr("Pantau dan kendalikan komputer di ruang kelas atau kantor, langsung dari genggaman.")
			muted: true
		}

		ColumnLayout {
			Layout.fillWidth: true
			Layout.topMargin: 22
			spacing: 14

			Repeater {
				model: [
					{ icon: "visibility", title: qsTr("Lihat layar secara langsung"), text: qsTr("Semua layar dalam satu grid, diperbarui otomatis.") },
					{ icon: "touch_app", title: qsTr("Kendalikan dari jarak jauh"), text: qsTr("Sentuh untuk klik, ketik lewat keyboard HP.") },
					{ icon: "lock", title: qsTr("Satu ketuk untuk semua"), text: qsTr("Kunci layar, kirim pesan, atau matikan sekaligus.") }
				]
				delegate: RowLayout {
					required property var modelData
					Layout.fillWidth: true
					spacing: 14
					Rectangle {
						width: 44; height: 44; radius: 14
						color: Theme.accentSoft
						Icon { anchors.centerIn: parent; name: modelData.icon; color: Theme.accent; size: 22 }
					}
					ColumnLayout {
						Layout.fillWidth: true
						spacing: 1
						AppText { Layout.fillWidth: true; text: modelData.title; style: "label"; font.pixelSize: Theme.fontBody }
						AppText { Layout.fillWidth: true; text: modelData.text; style: "caption"; muted: true }
					}
				}
			}
		}

		Item { Layout.fillHeight: true }

		AppButton {
			Layout.fillWidth: true
			text: qsTr("Mulai")
			iconName: "arrow_forward"
			onClicked: {
				Theme.prefs.onboardingDone = true
				window.openAuth(false)
			}
		}
	}
}
