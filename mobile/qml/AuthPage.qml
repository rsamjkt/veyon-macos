import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs

Page {
	id: page

	property bool fromSettings: false
	property string method: "key"   // "key" or "logon"
	property string error: ""
	property url pickedFile: ""

	function finish() {
		window.toast(qsTr("Akses tersambung. Komputer akan terhubung otomatis."), "success")
		if (fromSettings)
			StackView.view.pop()
		else
			window.goHome()
	}

	background: Rectangle { color: Theme.background }

	header: Rectangle {
		color: Theme.background
		implicitHeight: 64 + window.safeTop
		RowLayout {
			anchors.fill: parent
			anchors.topMargin: window.safeTop
			anchors.leftMargin: 6
			anchors.rightMargin: Theme.pad
			IconButton {
				iconName: "arrow_back"
				visible: page.StackView.index > 0
				onClicked: page.StackView.view.pop()
			}
			Item { Layout.fillWidth: true }
			AppButton {
				visible: !page.fromSettings
				variant: "ghost"
				compact: true
				text: qsTr("Nanti saja")
				onClicked: window.goHome()
			}
		}
	}

	Flickable {
		anchors.fill: parent
		clip: true
		contentHeight: column.implicitHeight + 40 + window.safeBottom
		boundsBehavior: Flickable.StopAtBounds

		ColumnLayout {
			id: column
			width: Math.min(parent.width, 560) - 2 * Theme.padLarge
			x: (parent.width - width) / 2
			spacing: 14

			AppText {
				Layout.fillWidth: true
				text: qsTr("Hubungkan akses")
				style: "display"
			}
			AppText {
				Layout.fillWidth: true
				text: qsTr("Komputer hanya menerima perintah dari perangkat yang punya izin. Pilih cara yang sama dengan pengaturan di komputer guru/admin.")
				muted: true
			}

			// --- option: key file
			Card {
				Layout.fillWidth: true
				Layout.topMargin: 8
				implicitHeight: keyColumn.implicitHeight + 32
				border.width: page.method === "key" ? 2 : (Theme.dark ? 0 : 1)
				border.color: page.method === "key" ? Theme.accent : Theme.border

				MouseArea { anchors.fill: parent; onClicked: page.method = "key" }

				ColumnLayout {
					id: keyColumn
					anchors.fill: parent
					anchors.margins: 16
					spacing: 12

					RowLayout {
						spacing: 14
						Rectangle {
							width: 48; height: 48; radius: 16
							color: Theme.accentSoft
							Icon { anchors.centerIn: parent; name: "key_fill"; color: Theme.accent; size: 26 }
						}
						ColumnLayout {
							Layout.fillWidth: true
							spacing: 2
							RowLayout {
								spacing: 8
								AppText { text: qsTr("Kunci akses"); style: "heading"; wrapMode: Text.NoWrap }
								Rectangle {
									implicitWidth: recommended.implicitWidth + 14; implicitHeight: 22; radius: 11
									color: Theme.successSoft
									AppText { id: recommended; anchors.centerIn: parent; text: qsTr("Disarankan"); style: "caption"; font.weight: Font.Bold; color: Theme.success; wrapMode: Text.NoWrap }
								}
							}
							AppText { Layout.fillWidth: true; text: qsTr("File …_private_key.pem dari AruniControl Configurator"); style: "caption"; muted: true }
						}
					}

					ColumnLayout {
						visible: page.method === "key"
						Layout.fillWidth: true
						spacing: 12

						Rectangle {
							Layout.fillWidth: true
							implicitHeight: steps.implicitHeight + 24
							radius: Theme.radius
							color: Theme.surfaceAlt
							ColumnLayout {
								id: steps
								anchors.fill: parent
								anchors.margins: 12
								spacing: 8
								Repeater {
									model: [
										qsTr("Di komputer guru, buka AruniControl Configurator → Authentication keys."),
										qsTr("Pilih kunci yang bertipe private, lalu tekan Export key."),
										qsTr("Kirim file .pem ke HP ini (WhatsApp, Drive, kabel USB), lalu pilih di bawah.")
									]
									delegate: RowLayout {
										required property string modelData
										required property int index
										Layout.fillWidth: true
										spacing: 10
										Rectangle {
											Layout.alignment: Qt.AlignTop
											width: 22; height: 22; radius: 11
											color: Theme.accent
											AppText { anchors.centerIn: parent; text: index + 1; style: "caption"; font.weight: Font.Bold; color: Theme.textOnAccent }
										}
										AppText { Layout.fillWidth: true; text: modelData; style: "caption"; font.pixelSize: 13 }
									}
								}
							}
						}

						AppButton {
							visible: page.pickedFile.toString().length === 0
							Layout.fillWidth: true
							text: qsTr("Pilih file kunci")
							iconName: "folder_open"
							onClicked: fileDialog.open()
						}

						// confirm the key name - it must match the key on the computers
						ColumnLayout {
							visible: page.pickedFile.toString().length > 0
							Layout.fillWidth: true
							spacing: 8

							RowLayout {
								Layout.fillWidth: true
								spacing: 8
								Icon { name: "check_circle_fill"; color: Theme.success; size: 20 }
								AppText {
									Layout.fillWidth: true
									text: App.displayName(page.pickedFile)
									style: "label"
									elide: Text.ElideMiddle
									wrapMode: Text.NoWrap
								}
								AppButton { variant: "ghost"; compact: true; text: qsTr("Ganti"); onClicked: fileDialog.open() }
							}
							InputField {
								id: keyName
								Layout.fillWidth: true
								iconName: "key"
								placeholderText: qsTr("Nama kunci")
								inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
							}
							AppText {
								Layout.fillWidth: true
								text: qsTr("Harus sama persis dengan nama kunci di komputer (kolom Name di Configurator → Authentication keys).")
								style: "caption"
								muted: true
							}
							AppButton {
								Layout.fillWidth: true
								text: qsTr("Pasang kunci")
								iconName: "check"
								enabled: keyName.text.trim().length > 0
								onClicked: {
									page.error = App.importKeyFile(page.pickedFile, keyName.text.trim())
									if (page.error.length === 0)
										page.finish()
								}
							}
						}
					}
				}
			}

			// --- option: logon
			Card {
				Layout.fillWidth: true
				implicitHeight: logonColumn.implicitHeight + 32
				border.width: page.method === "logon" ? 2 : (Theme.dark ? 0 : 1)
				border.color: page.method === "logon" ? Theme.accent : Theme.border

				MouseArea { anchors.fill: parent; onClicked: page.method = "logon" }

				ColumnLayout {
					id: logonColumn
					anchors.fill: parent
					anchors.margins: 16
					spacing: 12

					RowLayout {
						spacing: 14
						Rectangle {
							width: 48; height: 48; radius: 16
							color: Theme.infoSoft
							Icon { anchors.centerIn: parent; name: "person_fill"; color: Theme.info; size: 26 }
						}
						ColumnLayout {
							Layout.fillWidth: true
							spacing: 2
							AppText { text: qsTr("Akun komputer"); style: "heading" }
							AppText { Layout.fillWidth: true; text: qsTr("Nama pengguna & kata sandi yang terdaftar di komputer siswa"); style: "caption"; muted: true }
						}
					}

					ColumnLayout {
						visible: page.method === "logon"
						Layout.fillWidth: true
						spacing: 10

						InputField {
							id: username
							Layout.fillWidth: true
							iconName: "person"
							placeholderText: qsTr("Nama pengguna")
							inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
						}
						InputField {
							id: password
							Layout.fillWidth: true
							iconName: "key"
							placeholderText: qsTr("Kata sandi")
							echoMode: TextInput.Password
							onAccepted: logonButton.clicked()
						}
						CheckBox {
							id: remember
							checked: true
							text: qsTr("Ingat di perangkat ini")
							font.family: Theme.fontFamily
							font.pixelSize: Theme.fontLabel
						}
						AppButton {
							id: logonButton
							Layout.fillWidth: true
							text: qsTr("Masuk")
							iconName: "check"
							enabled: username.text.length > 0 && password.text.length > 0
							onClicked: {
								page.error = App.useLogon(username.text, password.text, remember.checked)
								if (page.error.length === 0)
									page.finish()
							}
						}
					}
				}
			}

			// error banner
			Rectangle {
				visible: page.error.length > 0
				Layout.fillWidth: true
				implicitHeight: errorRow.implicitHeight + 24
				radius: Theme.radius
				color: Theme.dangerSoft
				RowLayout {
					id: errorRow
					anchors.fill: parent
					anchors.margins: 12
					spacing: 10
					Icon { name: "error"; color: Theme.danger; size: 22; Layout.alignment: Qt.AlignTop }
					AppText { Layout.fillWidth: true; text: page.error; color: Theme.danger; style: "label"; font.weight: Font.Medium }
				}
			}

			AppText {
				Layout.fillWidth: true
				Layout.topMargin: 6
				text: qsTr("Kunci dan kata sandi hanya disimpan di HP ini dan tidak pernah dikirim ke internet.")
				style: "caption"
				muted: true
				horizontalAlignment: Text.AlignHCenter
			}
		}
	}

	FileDialog {
		id: fileDialog
		title: qsTr("Pilih file kunci privat")
		fileMode: FileDialog.OpenFile
		onAccepted: {
			page.error = ""
			page.pickedFile = selectedFile
			keyName.text = App.suggestKeyName(selectedFile)
		}
	}
}
