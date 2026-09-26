import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

// Log akses: who connected to one computer, when, and which functions were used
Page {
	id: page

	property string computerUid
	readonly property var log: App.accessLog
	readonly property var entries: log.computerUid === computerUid ? log.entries : []
	readonly property string logState: log.computerUid === computerUid ? log.state : "loading"
	readonly property real pullDistance: Math.max(0, -list.verticalOvershoot)
	readonly property bool pulledEnough: list.maxPull > 56
	property bool refreshing: false

	function kindColor(kind) {
		switch (kind) {
		case "ok": return Theme.success
		case "danger": return Theme.danger
		case "warning": return Theme.accent
		}
		return Theme.info
	}

	function refresh() {
		refreshing = true
		log.refresh()
	}

	Component.onCompleted: log.open(computerUid)
	Component.onDestruction: if (log.computerUid === computerUid) log.close()

	Connections {
		target: page.log
		function onEntriesChanged() { page.refreshing = false }
		function onStateChanged() { if (page.log.state !== "ready" && page.log.state !== "loading") page.refreshing = false }
	}

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
				AppText { Layout.fillWidth: true; text: qsTr("Log akses"); style: "heading"; wrapMode: Text.NoWrap; elide: Text.ElideRight }
				AppText { Layout.fillWidth: true; text: page.log.computerName; style: "caption"; muted: true; wrapMode: Text.NoWrap; elide: Text.ElideRight }
			}
			IconButton {
				iconName: "refresh"
				tooltip: qsTr("Muat ulang")
				onClicked: page.refresh()
			}
		}
	}

	// pull-to-refresh hint above the list
	RowLayout {
		anchors.horizontalCenter: parent.horizontalCenter
		y: Math.min(page.pullDistance, 80) / 2 - height / 2 + 8
		visible: list.visible && (page.pullDistance > 8 || page.refreshing)
		opacity: page.refreshing ? 1 : Math.min(1, page.pullDistance / 56)
		spacing: 8
		BusyIndicator { implicitWidth: 22; implicitHeight: 22; running: page.refreshing; visible: page.refreshing }
		Icon { visible: !page.refreshing; name: "arrow_downward"; size: 18; color: Theme.textMuted; rotation: page.pulledEnough ? 180 : 0 }
		AppText {
			text: page.refreshing ? qsTr("Memuat…") : (page.pulledEnough ? qsTr("Lepas untuk memuat ulang") : qsTr("Tarik untuk memuat ulang"))
			style: "caption"
			muted: true
		}
	}

	ListView {
		id: list
		anchors.fill: parent
		anchors.leftMargin: window.safeLeft
		anchors.rightMargin: window.safeRight
		anchors.topMargin: page.refreshing ? 36 : 0
		Behavior on anchors.topMargin { NumberAnimation { duration: Theme.fast } }
		clip: true
		bottomMargin: 20 + window.safeBottom
		visible: page.logState === "ready" && page.entries.length > 0
		model: page.entries
		boundsBehavior: Flickable.DragOverBounds
		flickableDirection: Flickable.VerticalFlick
		// pull to refresh: remember how far the list was pulled during the drag
		property real maxPull: 0
		onVerticalOvershootChanged: if (dragging) maxPull = Math.max(maxPull, -verticalOvershoot)
		onDraggingChanged: {
			if (!dragging && maxPull > 56)
				page.refresh()
			maxPull = 0
		}

		header: AppText {
			width: ListView.view.width
			leftPadding: Theme.pad + 4
			topPadding: 4
			bottomPadding: 4
			text: qsTr("%1 catatan, terbaru di atas").arg(page.entries.length) +
				  (page.log.updatedAt.length > 0 ? " · " + qsTr("diperbarui %1").arg(page.log.updatedAt) : "")
			style: "caption"
			muted: true
		}

		delegate: ColumnLayout {
			id: entry
			required property var modelData
			required property int index
			// day header above the first entry of each day
			readonly property bool firstOfDay: index === 0 || page.entries[index - 1].day !== modelData.day
			readonly property color tint: page.kindColor(modelData.kind)
			width: ListView.view.width
			spacing: 0

			AppText {
				visible: entry.firstOfDay
				Layout.fillWidth: true
				leftPadding: Theme.pad + 4
				topPadding: 14
				bottomPadding: 4
				text: entry.modelData.day
				style: "label"
				font.weight: Font.Bold
			}

			RowLayout {
				Layout.fillWidth: true
				Layout.preferredHeight: Math.max(64, texts.implicitHeight + 20)
				spacing: 14

				Rectangle {
					Layout.leftMargin: Theme.pad
					width: 40; height: 40; radius: 12
					color: Qt.rgba(entry.tint.r, entry.tint.g, entry.tint.b, Theme.dark ? 0.22 : 0.13)
					Icon { anchors.centerIn: parent; name: entry.modelData.icon; color: entry.tint; size: 22 }
				}
				ColumnLayout {
					id: texts
					Layout.fillWidth: true
					spacing: 2
					AppText {
						Layout.fillWidth: true
						text: entry.modelData.title
						style: "label"
						font.pixelSize: Theme.fontBody
						elide: Text.ElideRight
						maximumLineCount: 2
					}
					AppText {
						Layout.fillWidth: true
						text: entry.modelData.detail
						style: "caption"
						muted: true
						elide: Text.ElideRight
						wrapMode: Text.NoWrap
					}
				}
				AppText {
					Layout.rightMargin: Theme.pad
					text: entry.modelData.time
					style: "caption"
					muted: true
					font.weight: Font.DemiBold
				}
			}
		}
	}

	// ------------------------------------------------------------ loading / empty / problems
	ColumnLayout {
		visible: page.logState === "loading" || page.logState === ""
		anchors.centerIn: parent
		spacing: 14
		BusyIndicator { Layout.alignment: Qt.AlignHCenter; running: parent.visible }
		AppText { Layout.alignment: Qt.AlignHCenter; text: qsTr("Mengambil log akses…"); muted: true }
	}

	EmptyState {
		visible: page.logState === "offline" || page.logState === "noresponse" || (page.logState === "ready" && page.entries.length === 0)
		anchors.centerIn: parent
		width: Math.min(parent.width - 48, 400)
		iconName: page.logState === "offline" ? "wifi_off" : (page.logState === "noresponse" ? "error" : "history")
		title: page.logState === "offline" ? qsTr("Komputer tidak terhubung")
										   : (page.logState === "noresponse" ? qsTr("Komputer tidak merespons") : qsTr("Belum ada catatan"))
		text: page.logState === "offline" ? qsTr("Log akses bisa dibuka lagi begitu %1 online.").arg(page.log.computerName)
										  : (page.logState === "noresponse"
											 ? qsTr("Log akses hanya tersedia di komputer dengan AruniControl versi terbaru.")
											 : qsTr("Belum ada yang mengakses komputer ini sejak log akses aktif."))
		AppButton {
			variant: "tonal"
			text: qsTr("Coba lagi")
			iconName: "refresh"
			onClicked: page.refresh()
		}
	}
}
