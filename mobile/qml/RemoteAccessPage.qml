import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs

// Reaching computers when the phone is not on their network (mobile data,
// another Wi-Fi): embedded WireGuard, the ZeroTier app and extra subnets
Page {
	id: page

	readonly property var vpn: App.vpn
	property string error: ""

	background: Rectangle { color: Theme.background }

	Timer {
		// catch VPN changes made outside the app (ZeroTier, system settings)
		running: page.visible
		interval: 3000
		repeat: true
		onTriggered: page.vpn.refresh()
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
			AppText { Layout.fillWidth: true; text: qsTr("Akses jarak jauh"); style: "title"; wrapMode: Text.NoWrap }
		}
	}

	component Section: ColumnLayout {
		property string title
		property string hint
		default property alias rows: card.data
		Layout.fillWidth: true
		spacing: 8
		AppText { text: parent.title; style: "label"; muted: true; Layout.leftMargin: 8 }
		Card {
			Layout.fillWidth: true
			implicitHeight: card.implicitHeight + 32
			ColumnLayout {
				id: card
				x: 16; y: 16
				width: parent.width - 32
				spacing: 12
			}
		}
		AppText {
			visible: parent.hint.length > 0
			Layout.fillWidth: true
			Layout.leftMargin: 8
			Layout.rightMargin: 8
			text: parent.hint
			style: "caption"
			muted: true
		}
	}

	Flickable {
		anchors.fill: parent
		clip: true
		contentHeight: column.implicitHeight + 40 + window.safeBottom
		boundsBehavior: Flickable.StopAtBounds

		ColumnLayout {
			id: column
			width: Math.min(parent.width, 640) - 2 * Theme.pad
			x: (parent.width - width) / 2
			y: 4
			spacing: 22

			AppText {
				Layout.fillWidth: true
				Layout.leftMargin: 8
				text: qsTr("Kendalikan komputer walau HP memakai paket data atau Wi-Fi lain, lewat VPN ke jaringan sekolah/kantor.")
				muted: true
			}

			// ---------------------------------------------------- WireGuard
			Section {
				title: qsTr("VPN WireGuard (bawaan aplikasi)")
				hint: qsTr("Hanya AruniControl yang lewat VPN; aplikasi lain tetap memakai internet biasa. Cocok untuk MikroTik RouterOS 7 dan server WireGuard lain.")

				// status
				RowLayout {
					Layout.fillWidth: true
					spacing: 14
					Rectangle {
						width: 52; height: 52; radius: 18
						color: page.vpn.state === "on" ? Theme.successSoft : (page.vpn.state === "error" ? Theme.dangerSoft : Theme.surfaceAlt)
						Icon {
							anchors.centerIn: parent
							name: page.vpn.state === "on" ? "vpn_lock_fill" : "vpn_lock"
							color: page.vpn.state === "on" ? Theme.success : (page.vpn.state === "error" ? Theme.danger : Theme.textMuted)
							size: 28
						}
					}
					ColumnLayout {
						Layout.fillWidth: true
						spacing: 2
						AppText {
							Layout.fillWidth: true
							style: "heading"
							text: {
								if (!page.vpn.hasConfig) return qsTr("Belum diatur")
								switch (page.vpn.state) {
								case "on": return qsTr("Tersambung")
								case "connecting": return qsTr("Menyambungkan…")
								case "error": return qsTr("Gagal tersambung")
								default: return qsTr("Tidak tersambung")
								}
							}
						}
						AppText {
							Layout.fillWidth: true
							style: "caption"
							muted: true
							visible: text.length > 0
							text: page.vpn.state === "error" ? page.vpn.error
																: (page.vpn.hasConfig ? qsTr("Server: %1").arg(page.vpn.endpoint) : "")
							color: page.vpn.state === "error" ? Theme.danger : Theme.textMuted
						}
					}
					Switch {
						visible: page.vpn.hasConfig
						checked: page.vpn.state === "on" || page.vpn.state === "connecting"
						onToggled: checked ? page.vpn.connectTunnel() : page.vpn.disconnectTunnel()
					}
				}

				// details
				GridLayout {
					visible: page.vpn.hasConfig
					Layout.fillWidth: true
					columns: 2
					columnSpacing: 12
					rowSpacing: 6
					AppText { text: qsTr("Alamat HP"); style: "caption"; muted: true }
					AppText { Layout.fillWidth: true; text: page.vpn.address; style: "caption"; font.weight: Font.DemiBold }
					AppText { text: qsTr("Jaringan dipindai"); style: "caption"; muted: true }
					AppText {
						Layout.fillWidth: true
						text: page.vpn.tunnelSubnets.length > 0 ? page.vpn.tunnelSubnets.join(", ") : qsTr("— (tambahkan di bawah)")
						style: "caption"
						font.weight: Font.DemiBold
					}
				}

				RowLayout {
					visible: page.vpn.hasConfig
					Layout.fillWidth: true
					AppText { Layout.fillWidth: true; text: qsTr("Sambung otomatis saat aplikasi dibuka"); style: "label"; font.weight: Font.Medium }
					Switch {
						checked: page.vpn.autoConnect
						onToggled: page.vpn.autoConnect = checked
					}
				}

				RowLayout {
					Layout.fillWidth: true
					spacing: 10
					AppButton {
						Layout.fillWidth: true
						variant: page.vpn.hasConfig ? "outline" : "filled"
						compact: page.vpn.hasConfig
						text: page.vpn.hasConfig ? qsTr("Ganti konfigurasi") : qsTr("Impor file .conf")
						iconName: "upload_file"
						onClicked: confDialog.open()
					}
					AppButton {
						Layout.fillWidth: true
						variant: "outline"
						compact: page.vpn.hasConfig
						text: qsTr("Tempel teks")
						iconName: "content_paste"
						onClicked: pasteSheet.open()
					}
				}
				AppButton {
					visible: page.vpn.hasConfig
					Layout.fillWidth: true
					variant: "ghost"
					compact: true
					text: qsTr("Hapus konfigurasi VPN")
					iconName: "delete"
					onClicked: page.vpn.removeConfig()
				}

				AppText {
					visible: page.error.length > 0
					Layout.fillWidth: true
					text: page.error
					color: Theme.danger
					style: "label"
				}

				AppButton {
					Layout.fillWidth: true
					variant: "tonal"
					compact: true
					text: qsTr("Cara membuat di MikroTik")
					iconName: "router"
					onClicked: guideSheet.open()
				}
			}

			// ---------------------------------------------------- ZeroTier
			Section {
				title: qsTr("ZeroTier")
				hint: qsTr("Pakai aplikasi resmi ZeroTier One. Komputer di jaringan ZeroTier ditemukan otomatis setelah HP bergabung.")

				RowLayout {
					Layout.fillWidth: true
					spacing: 14
					Rectangle {
						width: 52; height: 52; radius: 18
						color: Theme.infoSoft
						Icon { anchors.centerIn: parent; name: "lan"; color: Theme.info; size: 28 }
					}
					ColumnLayout {
						Layout.fillWidth: true
						spacing: 2
						AppText { Layout.fillWidth: true; text: "ZeroTier One"; style: "heading" }
						AppText {
							Layout.fillWidth: true
							style: "caption"
							muted: true
							text: !page.vpn.supported ? qsTr("Hanya di Android")
													  : (page.vpn.zeroTierInstalled ? (page.vpn.otherVpnActive ? qsTr("VPN lain sedang aktif") : qsTr("Terpasang"))
																					: qsTr("Belum terpasang"))
						}
					}
				}
				Repeater {
					model: [
						qsTr("Pasang ZeroTier One dari Play Store."),
						qsTr("Buka, tekan +, lalu masukkan Network ID jaringan sekolah/kantor (16 karakter)."),
						qsTr("Admin menyetujui HP ini di my.zerotier.com (centang Auth)."),
						qsTr("Aktifkan jaringannya, lalu kembali ke AruniControl.")
					]
					delegate: RowLayout {
						required property string modelData
						required property int index
						Layout.fillWidth: true
						spacing: 10
						Rectangle {
							Layout.alignment: Qt.AlignTop
							width: 22; height: 22; radius: 11
							color: Theme.info
							AppText { anchors.centerIn: parent; text: index + 1; style: "caption"; font.weight: Font.Bold; color: "#FFFFFF" }
						}
						AppText { Layout.fillWidth: true; text: modelData; style: "caption"; font.pixelSize: 13 }
					}
				}
				AppButton {
					Layout.fillWidth: true
					variant: "tonal"
					text: page.vpn.zeroTierInstalled ? qsTr("Buka ZeroTier") : qsTr("Pasang ZeroTier One")
					iconName: "open_in_new"
					enabled: page.vpn.supported
					onClicked: page.vpn.openZeroTier()
				}
			}

			// ---------------------------------------------------- extra subnets
			Section {
				title: qsTr("Jaringan kantor yang dipindai")
				hint: qsTr("Jaringan tambahan yang dicari komputernya, mis. LAN di balik VPN atau router lain. Untuk WireGuard, jaringan dari AllowedIPs dipindai otomatis.")

				Repeater {
					model: page.vpn.extraSubnets
					delegate: RowLayout {
						required property string modelData
						Layout.fillWidth: true
						spacing: 10
						Icon { name: "lan"; color: Theme.textMuted; size: 20 }
						AppText { Layout.fillWidth: true; text: modelData; style: "label"; font.weight: Font.Medium }
						IconButton {
							iconName: "close"
							iconSize: 20
							iconColor: Theme.textMuted
							onClicked: page.vpn.extraSubnets = page.vpn.extraSubnets.filter(s => s !== modelData)
						}
					}
				}

				RowLayout {
					Layout.fillWidth: true
					spacing: 8
					InputField {
						id: subnetField
						Layout.fillWidth: true
						placeholderText: "192.168.1.0/24"
						iconName: "lan"
						inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhPreferNumbers
						onAccepted: addSubnet.clicked()
					}
					AppButton {
						id: addSubnet
						compact: true
						text: qsTr("Tambah")
						enabled: subnetField.text.trim().length > 0
						onClicked: {
							const problem = page.vpn.validateSubnet(subnetField.text)
							if (problem.length > 0) {
								window.toast(problem, "error")
								return
							}
							page.vpn.extraSubnets = page.vpn.extraSubnets.concat([subnetField.text.trim()])
							subnetField.text = ""
						}
					}
				}
			}

			Rectangle {
				Layout.fillWidth: true
				implicitHeight: noteRow.implicitHeight + 24
				radius: Theme.radius
				color: Theme.accentSoft
				RowLayout {
					id: noteRow
					anchors.fill: parent
					anchors.margins: 12
					spacing: 12
					Icon { name: "info"; color: Theme.accent; size: 22; Layout.alignment: Qt.AlignTop }
					AppText {
						Layout.fillWidth: true
						text: qsTr("Android hanya mengizinkan satu VPN aktif. Jika ZeroTier atau VPN lain menyala, WireGuard AruniControl akan menggantikannya (dan sebaliknya). L2TP/PPTP tidak lagi didukung Android 12 ke atas — gunakan WireGuard.")
						style: "caption"
					}
				}
			}
		}
	}

	FileDialog {
		id: confDialog
		title: qsTr("Pilih file konfigurasi WireGuard")
		fileMode: FileDialog.OpenFile
		onAccepted: {
			page.error = page.vpn.importConfigFile(selectedFile)
			if (page.error.length === 0)
				window.toast(qsTr("Konfigurasi VPN tersimpan"), "success")
		}
	}

	Sheet {
		id: pasteSheet
		title: qsTr("Tempel konfigurasi")
		subtitle: qsTr("Salin teks dari MikroTik (show-client-config) atau dari admin")
		iconName: "content_paste"

		onOpened: confText.forceActiveFocus()

		TextArea {
			id: confText
			Layout.fillWidth: true
			Layout.preferredHeight: 220
			placeholderText: "[Interface]\nPrivateKey = …\nAddress = 10.99.0.2/32\n\n[Peer]\nPublicKey = …\nEndpoint = kantor.contoh.id:13231\nAllowedIPs = 192.168.88.0/24"
			wrapMode: TextEdit.WrapAnywhere
			font.family: "monospace"
			font.pixelSize: 13
			color: Theme.text
			placeholderTextColor: Theme.textFaint
			padding: 14
			inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
			background: Rectangle { radius: Theme.radius; color: Theme.surfaceAlt }
		}
		AppButton {
			Layout.fillWidth: true
			text: qsTr("Simpan & sambungkan")
			iconName: "vpn_lock"
			enabled: confText.text.trim().length > 0
			onClicked: {
				const problem = page.vpn.importConfigText(confText.text)
				if (problem.length > 0) {
					window.toast(problem, "error")
				} else {
					confText.text = ""
					pasteSheet.close()
					window.toast(qsTr("Konfigurasi VPN tersimpan"), "success")
				}
			}
		}
	}

	Sheet {
		id: guideSheet
		title: qsTr("WireGuard di MikroTik")
		subtitle: qsTr("RouterOS 7.18 atau lebih baru · dikerjakan admin jaringan")
		iconName: "router"

		AppText { Layout.fillWidth: true; text: qsTr("1. Buat interface WireGuard dan beri alamat:"); style: "label" }
		Rectangle {
			Layout.fillWidth: true
			implicitHeight: cmd1.implicitHeight + 24
			radius: Theme.radiusSmall
			color: Theme.surfaceAlt
			TextEdit {
				id: cmd1
				x: 12; y: 12
				width: parent.width - 24
				readOnly: true
				selectByMouse: true
				wrapMode: TextEdit.WrapAnywhere
				color: Theme.text
				font.family: "monospace"
				font.pixelSize: 12
				text: "/interface wireguard add name=wg-aruni listen-port=13231\n/ip address add address=10.99.0.1/24 interface=wg-aruni\n/ip firewall filter add chain=input protocol=udp dst-port=13231 action=accept place-before=0"
			}
		}
		AppText { Layout.fillWidth: true; text: qsTr("2. Tambahkan HP sebagai peer (satu per HP), lalu tampilkan konfigurasinya:"); style: "label" }
		Rectangle {
			Layout.fillWidth: true
			implicitHeight: cmd2.implicitHeight + 24
			radius: Theme.radiusSmall
			color: Theme.surfaceAlt
			TextEdit {
				id: cmd2
				x: 12; y: 12
				width: parent.width - 24
				readOnly: true
				selectByMouse: true
				wrapMode: TextEdit.WrapAnywhere
				color: Theme.text
				font.family: "monospace"
				font.pixelSize: 12
				text: "/interface wireguard peers add interface=wg-aruni name=hp-guru private-key=auto responder=yes allowed-address=10.99.0.2/32 client-address=10.99.0.2/32 client-endpoint=<IP-publik-atau-DDNS> client-keepalive=25s\n/interface wireguard peers show-client-config [find name=hp-guru]"
			}
		}
		AppText {
			Layout.fillWidth: true
			text: qsTr("3. Ubah AllowedIPs di konfigurasi klien menjadi jaringan komputer (mis. 192.168.88.0/24), kirim teksnya ke HP ini, lalu pilih \"Tempel teks\". Router harus punya IP publik/DDNS dan port UDP 13231 terbuka.")
			style: "caption"
			muted: true
		}
	}
}
