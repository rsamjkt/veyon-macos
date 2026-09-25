import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Page {
	id: page

	background: Rectangle { color: Theme.background }

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
			AppText { Layout.fillWidth: true; text: qsTr("Pengaturan"); style: "title"; wrapMode: Text.NoWrap }
		}
	}

	component Section: ColumnLayout {
		property string title
		default property alias rows: card.data
		Layout.fillWidth: true
		spacing: 8
		AppText { text: parent.title; style: "label"; muted: true; Layout.leftMargin: 8 }
		Card {
			Layout.fillWidth: true
			implicitHeight: card.implicitHeight
			clip: true
			ColumnLayout {
				id: card
				width: parent.width
				spacing: 0
			}
		}
	}

	Flickable {
		anchors.fill: parent
		clip: true
		contentHeight: column.implicitHeight + 32 + window.safeBottom
		boundsBehavior: Flickable.StopAtBounds

		ColumnLayout {
			id: column
			width: Math.min(parent.width, 640) - 2 * Theme.pad
			x: (parent.width - width) / 2
			y: 8
			spacing: 22

			Section {
				title: qsTr("Akses")
				ListRow {
					Layout.fillWidth: true
					iconName: App.authenticated ? (App.authMethod === "key" ? "key_fill" : "person_fill") : "key"
					iconColor: App.authenticated ? Theme.success : Theme.warning
					title: App.authenticated ? (App.authMethod === "key" ? qsTr("Kunci akses: %1").arg(App.authName)
																		 : qsTr("Akun: %1").arg(App.authName))
											 : qsTr("Akses belum diatur")
					subtitle: App.authenticated ? qsTr("Ketuk untuk mengganti") : qsTr("Pasang kunci akses atau akun agar bisa terhubung")
					onClicked: window.openAuth(true)
				}
				ListRow {
					Layout.fillWidth: true
					visible: App.authenticated
					iconName: "logout"
					iconColor: Theme.danger
					title: qsTr("Hapus akses dari HP ini")
					danger: true
					chevron: false
					onClicked: signOutSheet.open()
				}
			}

			Section {
				title: qsTr("Koneksi")
				ListRow {
					Layout.fillWidth: true
					iconName: App.vpn.state === "on" ? "vpn_lock_fill" : "vpn_lock"
					iconColor: App.vpn.state === "on" ? Theme.success : Theme.info
					title: qsTr("Akses jarak jauh (VPN)")
					subtitle: App.vpn.state === "on" ? qsTr("WireGuard tersambung ke %1").arg(App.vpn.endpoint)
													 : qsTr("Kendalikan lewat paket data: WireGuard/MikroTik, ZeroTier")
					onClicked: window.openRemoteAccess()
				}
			}

			Section {
				title: qsTr("Komputer")
				ListRow {
					Layout.fillWidth: true
					iconName: "meeting_room"
					title: qsTr("Ruangan & komputer")
					subtitle: qsTr("Tambah komputer lewat alamat IP, kelompokkan per ruangan")
					value: App.rooms.length > 0 ? App.rooms.length : ""
					onClicked: window.openRooms()
				}
				ListRow {
					Layout.fillWidth: true
					iconName: "wifi_find"
					iconColor: Theme.info
					title: qsTr("Pindai ulang jaringan")
					subtitle: App.networkAddress.length > 0 ? qsTr("HP ini: %1").arg(App.networkAddress) : qsTr("HP tidak terhubung ke jaringan")
					chevron: false
					onClicked: {
						App.refreshComputers()
						window.toast(qsTr("Memindai ulang jaringan…"), "info")
					}
				}
				ListRow {
					Layout.fillWidth: true
					iconName: "download"
					iconColor: Theme.accent
					title: qsTr("File terkumpul")
					subtitle: qsTr("File yang diambil dari komputer siswa")
					onClicked: window.openCollected()
				}
				ListRow {
					Layout.fillWidth: true
					iconName: "photo_library"
					iconColor: Theme.success
					title: qsTr("Tangkapan layar")
					onClicked: window.openScreenshots()
				}
			}

			Section {
				title: qsTr("Tampilan")
				ColumnLayout {
					Layout.fillWidth: true
					Layout.margins: Theme.pad
					spacing: 10
					AppText { text: qsTr("Tema"); style: "label" }
					RowLayout {
						Layout.fillWidth: true
						spacing: 8
						Repeater {
							model: [ { label: qsTr("Otomatis"), icon: "contrast" }, { label: qsTr("Terang"), icon: "light_mode" }, { label: qsTr("Gelap"), icon: "dark_mode" } ]
							delegate: Chip {
								required property var modelData
								required property int index
								Layout.fillWidth: true
								text: modelData.label
								iconName: index === 0 ? "" : modelData.icon
								selected: Theme.mode === index
								onClicked: Theme.setMode(index)
							}
						}
					}
					AppText { text: qsTr("Ukuran kartu komputer"); style: "label"; Layout.topMargin: 6 }
					RowLayout {
						Layout.fillWidth: true
						spacing: 8
						Repeater {
							model: [qsTr("Kecil"), qsTr("Sedang"), qsTr("Besar")]
							delegate: Chip {
								required property string modelData
								required property int index
								Layout.fillWidth: true
								text: modelData
								selected: Theme.prefs.gridSize === index
								onClicked: Theme.prefs.gridSize = index
							}
						}
					}
					AppButton {
						Layout.fillWidth: true
						Layout.topMargin: 4
						variant: "ghost"
						compact: true
						text: qsTr("Tampilkan lagi panduan kontrol")
						iconName: "help"
						onClicked: {
							Theme.prefs.remoteHintShown = false
							window.toast(qsTr("Panduan akan muncul saat mode Kontrol dibuka"), "info")
						}
					}
				}
			}

			Section {
				title: qsTr("Tentang")
				ColumnLayout {
					Layout.fillWidth: true
					Layout.margins: Theme.pad
					spacing: 10
					RowLayout {
						spacing: 14
						Image {
							source: Qt.resolvedUrl("images/logo.svg")
							sourceSize: Qt.size(56, 56)
							width: 56; height: 56
						}
						ColumnLayout {
							spacing: 2
							AppText { text: "AruniControl"; style: "heading" }
							AppText { text: qsTr("Versi %1").arg(App.version); style: "caption"; muted: true }
							AppText { text: App.deviceName; style: "caption"; muted: true; visible: App.deviceName.length > 0 }
						}
					}
					AppText {
						Layout.fillWidth: true
						text: qsTr("Oleh Arunika. Berbasis Veyon, perangkat lunak bebas berlisensi GNU GPL v2. Ikon: Material Symbols (Apache 2.0). Huruf: Plus Jakarta Sans (OFL).")
						style: "caption"
						muted: true
					}
				}
			}
		}
	}

	Sheet {
		id: signOutSheet
		title: qsTr("Hapus akses?")
		subtitle: qsTr("HP ini tidak akan bisa mengendalikan komputer sampai akses dipasang lagi.")
		iconName: "logout"
		iconColor: Theme.danger

		RowLayout {
			Layout.fillWidth: true
			spacing: 10
			AppButton { Layout.fillWidth: true; variant: "outline"; text: qsTr("Batal"); onClicked: signOutSheet.close() }
			AppButton {
				Layout.fillWidth: true
				variant: "danger"
				text: qsTr("Hapus")
				onClicked: {
					App.signOut()
					signOutSheet.close()
					window.toast(qsTr("Akses dihapus dari HP ini"), "info")
				}
			}
		}
	}
}
