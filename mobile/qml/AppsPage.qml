import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

// Running applications on one computer (ApplicationMonitoring plugin)
Page {
	id: page

	property string computerUid
	readonly property var monitor: App.appMonitor
	readonly property var apps: monitor.computerUid === computerUid ? monitor.applications : []
	readonly property string monitorState: monitor.computerUid === computerUid ? monitor.state : "loading"

	// stable tint per application name for the letter avatars
	function tint(name) {
		const palette = ["#F2812F", "#2F6FE4", "#1F9D55", "#9B51E0", "#D48E00", "#DC3B3F", "#0E9AA7", "#7A5C3E"]
		let hash = 0
		for (let i = 0; i < name.length; ++i)
			hash = (hash * 31 + name.charCodeAt(i)) >>> 0
		return palette[hash % palette.length]
	}

	function askTerminate(name) {
		confirmSheet.application = name
		confirmSheet.open()
	}

	Component.onCompleted: monitor.open(computerUid)
	Component.onDestruction: if (monitor.computerUid === computerUid) monitor.close()

	background: Rectangle { color: Theme.background }

	header: Rectangle {
		color: list.contentY > list.originY + 4 ? Theme.surface : Theme.background
		implicitHeight: 64 + window.safeTop
		Behavior on color { ColorAnimation { duration: Theme.normal } }

		RowLayout {
			anchors.fill: parent
			anchors.topMargin: window.safeTop
			anchors.leftMargin: 6 + window.safeLeft
			anchors.rightMargin: 8 + window.safeRight
			spacing: 6
			IconButton { iconName: "arrow_back"; onClicked: page.StackView.view.pop() }
			ColumnLayout {
				Layout.fillWidth: true
				spacing: 0
				AppText { Layout.fillWidth: true; text: qsTr("Aplikasi berjalan"); style: "heading"; wrapMode: Text.NoWrap; elide: Text.ElideRight }
				AppText { Layout.fillWidth: true; text: page.monitor.computerName; style: "caption"; muted: true; wrapMode: Text.NoWrap; elide: Text.ElideRight }
			}
			IconButton {
				iconName: "refresh"
				tooltip: qsTr("Muat ulang")
				onClicked: page.monitor.refresh()
			}
		}
	}

	ColumnLayout {
		id: summary
		visible: list.visible
		anchors.top: parent.top
		anchors.left: parent.left
		anchors.right: parent.right
		anchors.leftMargin: window.safeLeft
		anchors.rightMargin: window.safeRight
		spacing: 12

		// the application the user is working in right now
		Rectangle {
			visible: page.monitor.frontmost.length > 0
			Layout.fillWidth: true
			Layout.leftMargin: Theme.pad
			Layout.rightMargin: Theme.pad
			Layout.topMargin: 4
			implicitHeight: activeRow.implicitHeight + 32
			radius: Theme.radiusLarge
			gradient: Gradient {
				orientation: Gradient.Horizontal
				GradientStop { position: 0; color: Theme.gradientTop }
				GradientStop { position: 1; color: Theme.gradientBottom }
			}

			RowLayout {
				id: activeRow
				anchors.fill: parent
				anchors.margins: 16
				spacing: 14
				Rectangle {
					width: 52; height: 52; radius: 16
					color: "#33FFFFFF"
					Text {
						anchors.centerIn: parent
						text: page.monitor.frontmost.charAt(0).toUpperCase()
						color: "#FFFFFF"
						font.family: Theme.fontFamily
						font.pixelSize: 24
						font.weight: Font.ExtraBold
					}
				}
				ColumnLayout {
					Layout.fillWidth: true
					spacing: 2
					Text {
						text: qsTr("SEDANG DIPAKAI")
						color: "#E6FFFFFF"
						font.family: Theme.fontFamily
						font.pixelSize: 11
						font.weight: Font.Bold
						font.letterSpacing: 0.8
					}
					Text {
						Layout.fillWidth: true
						text: page.monitor.frontmost
						color: "#FFFFFF"
						font.family: Theme.fontFamily
						font.pixelSize: Theme.fontHeading
						font.weight: Font.Bold
						elide: Text.ElideRight
					}
				}
				AppButton {
					id: closeActive
					compact: true
					variant: "outline"
					text: qsTr("Tutup")
					onClicked: page.askTerminate(page.monitor.frontmost)
					// white outline on the gradient
					background: Rectangle {
						radius: height / 2
						color: closeActive.pressed ? "#33FFFFFF" : "transparent"
						border.width: 1.5
						border.color: "#CCFFFFFF"
					}
					contentItem: Text {
						text: qsTr("Tutup")
						color: "#FFFFFF"
						font.family: Theme.fontFamily
						font.pixelSize: Theme.fontLabel
						font.weight: Font.DemiBold
						horizontalAlignment: Text.AlignHCenter
						verticalAlignment: Text.AlignVCenter
					}
				}
			}
		}

		AppText {
			Layout.leftMargin: Theme.pad + 4
			Layout.bottomMargin: 2
			text: qsTr("%1 aplikasi terbuka").arg(page.apps.length) +
				  (page.monitor.updatedAt.length > 0 ? " · " + qsTr("diperbarui %1").arg(page.monitor.updatedAt) : "")
			style: "label"
			muted: true
			font.weight: Font.Medium
		}
	}

	ListView {
		id: list
		anchors.fill: parent
		anchors.topMargin: summary.visible ? summary.height + 8 : 0
		clip: true
		anchors.leftMargin: window.safeLeft
		anchors.rightMargin: window.safeRight
		bottomMargin: 20 + window.safeBottom
		visible: page.monitorState === "ready" && page.apps.length > 0
		model: page.apps
		// the list is replaced on every refresh - keep the user's scroll position
		property real keptY: 0
		onMovementEnded: keptY = contentY
		onModelChanged: Qt.callLater(() => contentY = Math.min(keptY, Math.max(originY, contentHeight - height + originY)))

		delegate: ListRow {
			id: appRow
			required property string modelData
			readonly property bool active: modelData === page.monitor.frontmost
			width: ListView.view.width
			leftPadding: Theme.pad + 4
			title: modelData
			subtitle: active ? qsTr("Sedang dipakai") : ""
			chevron: false

			// letter avatar in front of the title
			contentItem: RowLayout {
				spacing: 14
				Rectangle {
					width: 40; height: 40; radius: 12
					readonly property color c: page.tint(appRow.modelData)
					color: Qt.rgba(c.r, c.g, c.b, Theme.dark ? 0.25 : 0.14)
					Text {
						anchors.centerIn: parent
						text: appRow.modelData.charAt(0).toUpperCase()
						color: parent.c
						font.family: Theme.fontFamily
						font.pixelSize: 18
						font.weight: Font.ExtraBold
					}
				}
				ColumnLayout {
					Layout.fillWidth: true
					spacing: 2
					AppText {
						Layout.fillWidth: true
						text: appRow.modelData
						style: "label"
						font.pixelSize: Theme.fontBody
						font.weight: appRow.active ? Font.Bold : Font.DemiBold
						elide: Text.ElideRight
						wrapMode: Text.NoWrap
					}
					RowLayout {
						visible: appRow.active
						spacing: 4
						Rectangle { width: 7; height: 7; radius: 4; color: Theme.success }
						AppText { text: qsTr("Sedang dipakai"); style: "caption"; color: Theme.success; font.weight: Font.DemiBold }
					}
				}
				IconButton {
					iconName: "cancel"
					iconColor: Theme.danger
					iconSize: 22
					tooltip: qsTr("Tutup aplikasi")
					onClicked: page.askTerminate(appRow.modelData)
				}
			}
			onClicked: page.askTerminate(modelData)
		}
	}

	// ------------------------------------------------------------ loading / empty / problems
	ColumnLayout {
		visible: page.monitorState === "loading" || page.monitorState === ""
		anchors.centerIn: parent
		spacing: 14
		BusyIndicator { Layout.alignment: Qt.AlignHCenter; running: parent.visible }
		AppText { Layout.alignment: Qt.AlignHCenter; text: qsTr("Mengambil daftar aplikasi…"); muted: true }
	}

	EmptyState {
		visible: page.monitorState === "offline" || page.monitorState === "noresponse" || (page.monitorState === "ready" && page.apps.length === 0)
		anchors.centerIn: parent
		width: Math.min(parent.width - 48, 400)
		iconName: page.monitorState === "offline" ? "wifi_off" : (page.monitorState === "noresponse" ? "error" : "apps")
		title: page.monitorState === "offline" ? qsTr("Komputer tidak terhubung")
										: (page.monitorState === "noresponse" ? qsTr("Komputer tidak merespons") : qsTr("Tidak ada aplikasi terbuka"))
		text: page.monitorState === "offline" ? qsTr("Daftar aplikasi muncul lagi begitu %1 online.").arg(page.monitor.computerName)
									   : (page.monitorState === "noresponse"
										  ? qsTr("Pemantauan aplikasi hanya tersedia di komputer Windows dan macOS dengan AruniControl versi terbaru.")
										  : qsTr("Belum ada pengguna yang login, atau semua aplikasi sudah ditutup."))
		AppButton {
			visible: page.monitorState !== "ready"
			variant: "tonal"
			text: qsTr("Coba lagi")
			iconName: "refresh"
			onClicked: page.monitor.refresh()
		}
	}

	// ------------------------------------------------------------ confirmation
	Sheet {
		id: confirmSheet
		property string application
		title: qsTr("Tutup %1?").arg(application)
		subtitle: page.monitor.computerName
		iconName: "cancel"
		iconColor: Theme.danger

		AppText {
			Layout.fillWidth: true
			text: qsTr("Aplikasi akan ditutup paksa di komputer. Pekerjaan yang belum disimpan di aplikasi ini bisa hilang.")
			muted: true
		}

		RowLayout {
			Layout.fillWidth: true
			Layout.topMargin: 8
			spacing: 10
			AppButton {
				Layout.fillWidth: true
				variant: "outline"
				text: qsTr("Batal")
				onClicked: confirmSheet.close()
			}
			AppButton {
				Layout.fillWidth: true
				variant: "danger"
				text: qsTr("Tutup aplikasi")
				iconName: "cancel"
				onClicked: {
					if (page.monitor.terminate(confirmSheet.application))
						window.toast(qsTr("Menutup %1…").arg(confirmSheet.application), "success")
					else
						window.toast(qsTr("%1 sedang tidak terhubung.").arg(page.monitor.computerName), "error")
					confirmSheet.close()
				}
			}
		}
	}
}
