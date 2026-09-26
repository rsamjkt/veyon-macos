import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs

Page {
	id: page

	background: Rectangle { color: Theme.background }

	function addComputer(roomUid) {
		computerSheet.roomUid = roomUid
		computerSheet.open()
	}

	header: Rectangle {
		color: Theme.background
		implicitHeight: 64 + window.safeTop
		RowLayout {
			anchors.fill: parent
			anchors.topMargin: window.safeTop
			anchors.leftMargin: 6 + window.safeLeft
			anchors.rightMargin: Theme.pad + window.safeRight
			spacing: 6
			IconButton { iconName: "arrow_back"; onClicked: page.StackView.view.pop() }
			AppText { Layout.fillWidth: true; text: qsTr("Ruangan & komputer"); style: "title"; wrapMode: Text.NoWrap }
			// a whole list from Excel instead of adding computers one by one
			IconButton { iconName: "upload_file"; onClicked: importDialog.open() }
		}
	}

	FileDialog {
		id: importDialog
		title: qsTr("Pilih daftar komputer (CSV dari Excel)")
		fileMode: FileDialog.OpenFile
		onAccepted: {
			const result = App.importComputers(selectedFile)
			window.toast(result.message, result.ok ? "success" : "error")
		}
	}

	Flickable {
		anchors.fill: parent
		clip: true
		contentHeight: column.implicitHeight + 120 + window.safeBottom
		boundsBehavior: Flickable.StopAtBounds

		ColumnLayout {
			id: column
			width: Math.min(parent.width, 640) - 2 * Theme.pad
			x: (parent.width - width) / 2
			y: 8
			spacing: 16

			// explanation
			Rectangle {
				Layout.fillWidth: true
				implicitHeight: infoRow.implicitHeight + 24
				radius: Theme.radius
				color: Theme.infoSoft
				RowLayout {
					id: infoRow
					anchors.fill: parent
					anchors.margins: 12
					spacing: 12
					Icon { name: "wifi_find"; color: Theme.info; size: 24; Layout.alignment: Qt.AlignTop }
					AppText {
						Layout.fillWidth: true
						text: qsTr("Komputer di jaringan Wi-Fi yang sama ditemukan otomatis. Tambahkan manual untuk komputer di jaringan lain (mis. lewat VPN) atau untuk mengelompokkan per ruangan.")
						style: "caption"
					}
				}
			}

			Repeater {
				model: App.rooms
				delegate: Card {
					id: roomCard
					required property var modelData
					Layout.fillWidth: true
					implicitHeight: roomColumn.implicitHeight + 8
					clip: true

					ColumnLayout {
						id: roomColumn
						width: parent.width
						spacing: 0

						RowLayout {
							Layout.fillWidth: true
							Layout.margins: Theme.pad
							Layout.bottomMargin: 8
							spacing: 12
							Rectangle {
								width: 40; height: 40; radius: 12
								color: Theme.accentSoft
								Icon { anchors.centerIn: parent; name: "meeting_room_fill"; color: Theme.accent; size: 22 }
							}
							ColumnLayout {
								Layout.fillWidth: true
								spacing: 0
								AppText { Layout.fillWidth: true; text: roomCard.modelData.name; style: "heading"; elide: Text.ElideRight; wrapMode: Text.NoWrap }
								AppText { text: qsTr("%1 komputer").arg(roomCard.modelData.computers.length); style: "caption"; muted: true }
							}
							IconButton {
								iconName: "edit"
								onClicked: {
									roomSheet.roomUid = roomCard.modelData.uid
									roomSheet.initialName = roomCard.modelData.name
									roomSheet.open()
								}
							}
							IconButton {
								iconName: "delete"
								iconColor: Theme.danger
								onClicked: {
									deleteSheet.kind = "room"
									deleteSheet.uid = roomCard.modelData.uid
									deleteSheet.name = roomCard.modelData.name
									deleteSheet.open()
								}
							}
						}

						Repeater {
							model: roomCard.modelData.computers
							delegate: ListRow {
								required property var modelData
								Layout.fillWidth: true
								iconName: "computer"
								iconColor: Theme.textMuted
								title: modelData.name
								subtitle: modelData.host + (modelData.mac.length > 0 ? "  ·  " + modelData.mac : "")
								chevron: false
								IconButton {
									iconName: "close"
									iconSize: 20
									iconColor: Theme.textMuted
									onClicked: {
										deleteSheet.kind = "computer"
										deleteSheet.uid = modelData.uid
										deleteSheet.name = modelData.name
										deleteSheet.open()
									}
								}
							}
						}

						AppButton {
							Layout.fillWidth: true
							Layout.margins: Theme.pad
							Layout.topMargin: 6
							variant: "tonal"
							compact: true
							text: qsTr("Tambah komputer")
							iconName: "add"
							onClicked: page.addComputer(roomCard.modelData.uid)
						}
					}
				}
			}

			EmptyState {
				visible: App.rooms.length === 0
				Layout.fillWidth: true
				Layout.topMargin: 30
				iconName: "meeting_room"
				title: qsTr("Belum ada ruangan")
				text: qsTr("Buat ruangan (mis. \"Lab Komputer 1\"), lalu tambahkan komputernya - atau ketuk ikon impor di atas untuk memuat daftar dari Excel (kolom: Ruangan, Nama, Alamat IP, MAC).")
			}
		}
	}

	AppButton {
		anchors.right: parent.right
		anchors.bottom: parent.bottom
		anchors.rightMargin: Theme.padLarge + window.safeRight
		anchors.bottomMargin: Theme.padLarge + window.safeBottom
		text: qsTr("Ruangan baru")
		iconName: "add"
		onClicked: {
			roomSheet.roomUid = ""
			roomSheet.initialName = ""
			roomSheet.open()
		}
	}

	// ---------------------------------------------------------------- sheets
	Sheet {
		id: roomSheet
		property string roomUid
		property string initialName
		title: roomUid.length > 0 ? qsTr("Ubah nama ruangan") : qsTr("Ruangan baru")
		iconName: "meeting_room"

		onOpened: {
			roomName.text = initialName
			roomName.forceActiveFocus()
		}

		InputField {
			id: roomName
			Layout.fillWidth: true
			placeholderText: qsTr("Nama ruangan, mis. Lab Komputer 1")
			iconName: "meeting_room"
			onAccepted: roomSave.clicked()
		}
		AppButton {
			id: roomSave
			Layout.fillWidth: true
			text: qsTr("Simpan")
			enabled: roomName.text.trim().length > 0
			onClicked: {
				if (roomSheet.roomUid.length > 0) {
					App.renameRoom(roomSheet.roomUid, roomName.text)
					roomSheet.close()
				} else {
					const uid = App.addRoom(roomName.text)
					roomSheet.close()
					page.addComputer(uid)
				}
			}
		}
	}

	Sheet {
		id: computerSheet
		property string roomUid
		property string error
		title: qsTr("Tambah komputer")
		iconName: "computer"

		onOpened: {
			error = ""
			hostField.text = ""
			nameField.text = ""
			macField.text = ""
			hostField.forceActiveFocus()
		}

		InputField {
			id: hostField
			Layout.fillWidth: true
			placeholderText: qsTr("Alamat IP, mis. 192.168.1.20")
			iconName: "dns"
			inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhPreferNumbers
		}
		InputField {
			id: nameField
			Layout.fillWidth: true
			placeholderText: qsTr("Nama (opsional), mis. PC-01")
			iconName: "desktop_windows"
		}
		InputField {
			id: macField
			Layout.fillWidth: true
			placeholderText: qsTr("Alamat MAC (opsional, untuk Nyalakan)")
			iconName: "power"
			inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
		}
		AppText {
			visible: computerSheet.error.length > 0
			Layout.fillWidth: true
			text: computerSheet.error
			color: Theme.danger
			style: "label"
		}
		RowLayout {
			Layout.fillWidth: true
			spacing: 10
			AppButton {
				Layout.fillWidth: true
				variant: "outline"
				text: qsTr("Simpan & tambah lagi")
				enabled: hostField.text.trim().length > 0
				onClicked: {
					computerSheet.error = App.addComputer(computerSheet.roomUid, nameField.text, hostField.text, macField.text)
					if (computerSheet.error.length === 0) {
						window.toast(qsTr("%1 ditambahkan").arg(nameField.text.length > 0 ? nameField.text : hostField.text), "success")
						hostField.text = ""
						nameField.text = ""
						macField.text = ""
						hostField.forceActiveFocus()
					}
				}
			}
			AppButton {
				Layout.fillWidth: true
				text: qsTr("Simpan")
				enabled: hostField.text.trim().length > 0
				onClicked: {
					computerSheet.error = App.addComputer(computerSheet.roomUid, nameField.text, hostField.text, macField.text)
					if (computerSheet.error.length === 0)
						computerSheet.close()
				}
			}
		}
	}

	Sheet {
		id: deleteSheet
		property string kind
		property string uid
		property string name
		title: kind === "room" ? qsTr("Hapus ruangan \"%1\"?").arg(name) : qsTr("Hapus \"%1\"?").arg(name)
		subtitle: kind === "room" ? qsTr("Semua komputer di ruangan ini ikut terhapus dari daftar.") : qsTr("Komputer dihapus dari daftar manual.")
		iconName: "delete"
		iconColor: Theme.danger

		RowLayout {
			Layout.fillWidth: true
			spacing: 10
			AppButton { Layout.fillWidth: true; variant: "outline"; text: qsTr("Batal"); onClicked: deleteSheet.close() }
			AppButton {
				Layout.fillWidth: true
				variant: "danger"
				text: qsTr("Hapus")
				onClicked: {
					if (deleteSheet.kind === "room")
						App.removeRoom(deleteSheet.uid)
					else
						App.removeComputer(deleteSheet.uid)
					deleteSheet.close()
				}
			}
		}
	}
}
