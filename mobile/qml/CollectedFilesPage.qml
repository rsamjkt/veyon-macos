import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Page {
	id: page

	property var files: App.collectedFiles()

	function reload() { files = App.collectedFiles() }

	background: Rectangle { color: Theme.background }

	Timer { running: page.visible; interval: 3000; repeat: true; onTriggered: page.reload() }

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
			AppText { Layout.fillWidth: true; text: qsTr("File terkumpul"); style: "title"; wrapMode: Text.NoWrap }
			IconButton { iconName: "refresh"; onClicked: page.reload() }
		}
	}

	ListView {
		anchors.fill: parent
		bottomMargin: 20 + window.safeBottom
		model: page.files
		delegate: ListRow {
			required property string modelData
			width: ListView.view.width
			iconName: "description"
			title: modelData.split("/").pop()
			subtitle: modelData.replace(App.collectedFilesDirectory + "/", "").split("/").slice(0, -1).join(" / ")
			chevron: false
			IconButton { iconName: "share"; iconSize: 20; onClicked: App.shareFile(modelData) }
			IconButton {
				iconName: "delete"
				iconSize: 20
				iconColor: Theme.danger
				onClicked: {
					App.deleteFile(modelData)
					page.reload()
				}
			}
			onClicked: App.shareFile(modelData)
		}
	}

	EmptyState {
		visible: page.files.length === 0
		anchors.centerIn: parent
		width: Math.min(parent.width - 48, 400)
		iconName: "download"
		title: qsTr("Belum ada file")
		text: qsTr("Gunakan Aksi → Kumpulkan file untuk mengambil file dari komputer siswa.")
	}
}
