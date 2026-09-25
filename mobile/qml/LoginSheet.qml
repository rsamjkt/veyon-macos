import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

// Logs a user in on the target computers (Veyon "Log in")
Sheet {
	id: sheet

	property var targetUids: []

	title: qsTr("Login pengguna")
	subtitle: qsTr("Di %1 komputer").arg(App.targetCount(targetUids))
	iconName: "login"

	onOpened: user.forceActiveFocus()
	onClosed: password.text = ""

	InputField {
		id: user
		Layout.fillWidth: true
		iconName: "person"
		placeholderText: qsTr("Nama pengguna di komputer")
		inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
	}
	InputField {
		id: password
		Layout.fillWidth: true
		iconName: "key"
		placeholderText: qsTr("Kata sandi")
		echoMode: TextInput.Password
		onAccepted: go.clicked()
	}
	AppText {
		Layout.fillWidth: true
		text: qsTr("Kata sandi dikirim terenkripsi dan tidak disimpan di HP.")
		style: "caption"
		muted: true
	}
	AppButton {
		id: go
		Layout.fillWidth: true
		text: qsTr("Login")
		iconName: "login"
		enabled: user.text.trim().length > 0
		onClicked: {
			App.loginUser(user.text, password.text, sheet.targetUids)
			sheet.close()
		}
	}
}
