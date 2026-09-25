import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Page {
	id: page

	property var files: App.screenshots()

	function reload() { files = App.screenshots() }

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
			AppText { Layout.fillWidth: true; text: qsTr("Tangkapan layar"); style: "title"; wrapMode: Text.NoWrap }
		}
	}

	GridView {
		id: grid
		readonly property int columns: Math.max(2, Math.floor((width - 20) / 170))
		anchors.fill: parent
		leftMargin: 10
		rightMargin: 10
		bottomMargin: 20 + window.safeBottom
		cellWidth: Math.floor((width - 20) / columns)
		cellHeight: cellWidth * 10 / 16 + 34
		model: page.files
		delegate: Item {
			id: shot
			required property string modelData
			width: grid.cellWidth
			height: grid.cellHeight
			ColumnLayout {
				anchors.fill: parent
				anchors.margins: 6
				spacing: 4
				Rectangle {
					Layout.fillWidth: true
					Layout.fillHeight: true
					radius: 14
					color: Theme.screenBackground
					clip: true
					Image {
						anchors.fill: parent
						source: "file://" + shot.modelData
						fillMode: Image.PreserveAspectFit
						asynchronous: true
						sourceSize.width: 400
					}
				}
				Text {
					Layout.fillWidth: true
					text: shot.modelData.split("/").pop().replace(".png", "")
					color: Theme.textMuted
					font.family: Theme.fontFamily
					font.pixelSize: 11
					elide: Text.ElideMiddle
				}
			}
			TapHandler {
				onTapped: {
					viewer.path = shot.modelData
					viewer.open()
				}
			}
		}
	}

	EmptyState {
		visible: page.files.length === 0
		anchors.centerIn: parent
		width: Math.min(parent.width - 48, 400)
		iconName: "photo_library"
		title: qsTr("Belum ada tangkapan layar")
		text: qsTr("Tangkap layar dari menu Aksi atau saat melihat layar komputer.")
	}

	Sheet {
		id: viewer
		property string path
		title: path.split("/").pop()
		iconName: "image"

		Image {
			Layout.fillWidth: true
			Layout.preferredHeight: width * 10 / 16
			source: viewer.path.length > 0 ? "file://" + viewer.path : ""
			fillMode: Image.PreserveAspectFit
			asynchronous: true
		}
		RowLayout {
			Layout.fillWidth: true
			spacing: 10
			AppButton {
				Layout.fillWidth: true
				variant: "outline"
				text: qsTr("Hapus")
				iconName: "delete"
				onClicked: {
					App.deleteFile(viewer.path)
					viewer.close()
					page.reload()
				}
			}
			AppButton {
				Layout.fillWidth: true
				text: qsTr("Bagikan")
				iconName: "share"
				onClicked: App.shareFile(viewer.path)
			}
		}
	}
}
