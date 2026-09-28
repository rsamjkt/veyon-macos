import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

// Info & program: hardware, disks and network of one computer and its
// installed programs, which can be removed
Page {
	id: page

	property string computerUid
	readonly property var ctrl: App.computerInfo
	readonly property bool current: ctrl.computerUid === computerUid
	readonly property string infoState: current ? ctrl.state : "loading"
	property string filter: ""
	property string confirmId: ""

	Component.onCompleted: ctrl.open(computerUid)
	Component.onDestruction: if (ctrl.computerUid === computerUid) ctrl.close()

	Timer { id: confirmTimer; interval: 4000; onTriggered: page.confirmId = "" }

	background: Rectangle { color: Theme.background }

	header: Rectangle {
		color: Theme.background
		implicitHeight: 64 + window.safeTop
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
				AppText { Layout.fillWidth: true; text: qsTr("Info & program"); style: "heading"; wrapMode: Text.NoWrap; elide: Text.ElideRight }
				AppText { Layout.fillWidth: true; text: page.ctrl.computerName; style: "caption"; muted: true; wrapMode: Text.NoWrap; elide: Text.ElideRight }
			}
			IconButton { iconName: "refresh"; tooltip: qsTr("Muat ulang"); onClicked: page.ctrl.refresh() }
		}
	}

	component InfoRow: RowLayout {
		property string label
		property string value
		visible: value.length > 0
		Layout.fillWidth: true
		spacing: 12
		AppText { Layout.preferredWidth: 96; text: parent.label; style: "caption"; muted: true }
		AppText { Layout.fillWidth: true; text: parent.value; style: "label"; wrapMode: Text.Wrap }
	}

	Flickable {
		id: flick
		anchors.fill: parent
		anchors.leftMargin: window.safeLeft
		anchors.rightMargin: window.safeRight
		visible: page.infoState === "ready"
		contentHeight: content.implicitHeight + 24 + window.safeBottom
		clip: true

		ColumnLayout {
			id: content
			x: Theme.pad
			width: flick.width - 2 * Theme.pad
			spacing: 12

			Card {
				Layout.fillWidth: true
				implicitHeight: specs.implicitHeight + 28
				ColumnLayout {
					id: specs
					anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
					spacing: 8
					RowLayout {
						spacing: 8
						Icon { name: "memory"; color: Theme.accent; size: 20 }
						AppText { text: qsTr("Spesifikasi"); style: "label"; font.weight: Font.Bold }
					}
					InfoRow { label: qsTr("Model"); value: page.ctrl.info.model || "" }
					InfoRow { label: qsTr("Nomor seri"); value: page.ctrl.info.serial || "" }
					InfoRow { label: qsTr("Sistem"); value: page.ctrl.info.os || "" }
					InfoRow { label: qsTr("Prosesor"); value: page.ctrl.info.cpu || "" }
					InfoRow { label: qsTr("RAM"); value: page.ctrl.info.ram || "" }
					InfoRow { label: qsTr("Pengguna"); value: page.ctrl.info.user || "" }
					InfoRow { label: qsTr("Menyala"); value: page.ctrl.info.uptime || "" }
					InfoRow { label: qsTr("AruniControl"); value: page.ctrl.info.version || "" }
				}
			}

			Card {
				Layout.fillWidth: true
				visible: page.ctrl.disks.length > 0
				implicitHeight: disks.implicitHeight + 28
				ColumnLayout {
					id: disks
					anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
					spacing: 10
					RowLayout {
						spacing: 8
						Icon { name: "storage"; color: Theme.accent; size: 20 }
						AppText { text: qsTr("Disk"); style: "label"; font.weight: Font.Bold }
					}
					Repeater {
						model: page.ctrl.disks
						delegate: ColumnLayout {
							required property var modelData
							Layout.fillWidth: true
							spacing: 4
							RowLayout {
								Layout.fillWidth: true
								AppText { text: modelData.path; style: "label" }
								Item { Layout.fillWidth: true }
								AppText { text: qsTr("%1 kosong dari %2").arg(modelData.free).arg(modelData.total); style: "caption"; muted: true }
							}
							Rectangle {
								Layout.fillWidth: true
								height: 8; radius: 4
								color: Theme.surfaceAlt
								Rectangle {
									width: parent.width * Math.min(100, modelData.usedPercent) / 100
									height: parent.height; radius: 4
									color: modelData.usedPercent >= 90 ? Theme.danger : Theme.accent
								}
							}
						}
					}
				}
			}

			Card {
				Layout.fillWidth: true
				visible: page.ctrl.network.length > 0
				implicitHeight: nets.implicitHeight + 28
				ColumnLayout {
					id: nets
					anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
					spacing: 8
					RowLayout {
						spacing: 8
						Icon { name: "lan"; color: Theme.accent; size: 20 }
						AppText { text: qsTr("Jaringan"); style: "label"; font.weight: Font.Bold }
					}
					Repeater {
						model: page.ctrl.network
						delegate: InfoRow { required property var modelData; label: modelData.name; value: modelData.ip + "\n" + modelData.mac }
					}
				}
			}

			RowLayout {
				Layout.topMargin: 4
				spacing: 8
				Icon { name: "apps"; color: Theme.accent; size: 20 }
				AppText { text: qsTr("Program (%1)").arg(page.ctrl.programs.length); style: "label"; font.weight: Font.Bold }
			}
			InputField {
				Layout.fillWidth: true
				iconName: "search"
				placeholderText: qsTr("Cari program")
				onTextChanged: page.filter = text.toLowerCase()
			}
			Repeater {
				model: page.ctrl.programs
				delegate: RowLayout {
					id: row
					required property var modelData
					visible: page.filter.length === 0 || modelData.name.toLowerCase().indexOf(page.filter) >= 0
					Layout.fillWidth: true
					Layout.preferredHeight: 52
					spacing: 10
					ColumnLayout {
						Layout.fillWidth: true
						spacing: 1
						AppText { Layout.fillWidth: true; text: row.modelData.name; style: "label"; elide: Text.ElideRight; wrapMode: Text.NoWrap }
						AppText {
							Layout.fillWidth: true
							text: row.modelData.state === "removing" ? qsTr("Menghapus…")
								: row.modelData.state === "removed" ? qsTr("Dihapus")
								: row.modelData.state.length > 0 ? row.modelData.state : row.modelData.version
							style: "caption"
							color: row.modelData.state === "removed" ? Theme.success
								 : (row.modelData.state.length > 0 && row.modelData.state !== "removing" ? Theme.danger : Theme.textMuted)
							elide: Text.ElideRight
							wrapMode: Text.NoWrap
						}
					}
					AppButton {
						visible: row.modelData.removable && row.modelData.state !== "removed" && row.modelData.state !== "removing"
						variant: page.confirmId === row.modelData.id ? "danger" : "outline"
						text: page.confirmId === row.modelData.id ? qsTr("Yakin hapus?") : qsTr("Hapus")
						iconName: "delete_forever"
						onClicked: {
							if (page.confirmId === row.modelData.id) {
								page.confirmId = ""
								if (!page.ctrl.uninstall(row.modelData.id))
									window.toast(qsTr("Komputer tidak terhubung."), "error")
							} else {
								page.confirmId = row.modelData.id
								confirmTimer.restart()
							}
						}
					}
				}
			}
			AppText {
				Layout.fillWidth: true
				text: qsTr("Program dihapus tanpa pertanyaan ke pengguna. Di Mac aplikasi dipindahkan ke Tempat Sampah.")
				style: "caption"
				muted: true
			}
		}
	}

	ColumnLayout {
		visible: page.infoState === "loading" || page.infoState === ""
		anchors.centerIn: parent
		spacing: 14
		BusyIndicator { Layout.alignment: Qt.AlignHCenter; running: parent.visible }
		AppText { Layout.alignment: Qt.AlignHCenter; text: qsTr("Mengambil info komputer…"); muted: true }
	}

	EmptyState {
		visible: page.infoState === "offline" || page.infoState === "noresponse"
		anchors.centerIn: parent
		width: Math.min(parent.width - 48, 400)
		iconName: page.infoState === "offline" ? "wifi_off" : "error"
		title: page.infoState === "offline" ? qsTr("Komputer tidak terhubung") : qsTr("Komputer tidak merespons")
		text: page.infoState === "offline" ? qsTr("Info bisa dibuka lagi begitu %1 online.").arg(page.ctrl.computerName)
										   : qsTr("Info komputer hanya tersedia di komputer dengan AruniControl 1.7 atau lebih baru.")
		AppButton { variant: "tonal"; text: qsTr("Coba lagi"); iconName: "refresh"; onClicked: page.ctrl.refresh() }
	}
}
