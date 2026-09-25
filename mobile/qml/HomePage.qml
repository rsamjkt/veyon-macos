import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Page {
	id: page

	readonly property var computers: App.computers
	property bool selectionMode: computers.selectedCount > 0
	// an empty list means "every computer currently shown"
	function targets() { return selectionMode ? computers.selectedUids() : [] }
	readonly property int targetCount: selectionMode ? computers.selectedCount : computers.count
	readonly property string targetLabel: selectionMode ? qsTr("%1 dipilih").arg(computers.selectedCount)
													   : qsTr("Semua (%1)").arg(computers.count)

	function openActions() {
		actionSheet.targetUids = page.targets()
		actionSheet.targetLabel = page.selectionMode ? qsTr("%1 komputer dipilih").arg(computers.selectedCount)
													 : qsTr("Semua %1 komputer yang tampil").arg(computers.count)
		actionSheet.open()
	}

	background: Rectangle { color: Theme.background }

	// ---------------------------------------------------------------- top bar
	header: Rectangle {
		implicitHeight: 60 + window.safeTop
		color: grid.contentY > grid.originY + 4 || page.selectionMode ? Theme.surface : Theme.background
		Behavior on color { ColorAnimation { duration: Theme.normal } }

		Rectangle {
			anchors.bottom: parent.bottom
			width: parent.width; height: 1
			color: Theme.border
			visible: grid.contentY > grid.originY + 4 && !page.selectionMode
		}

		// normal bar
		RowLayout {
			anchors.fill: parent
			anchors.topMargin: window.safeTop
			anchors.leftMargin: Theme.pad + window.safeLeft
			anchors.rightMargin: 8 + window.safeRight
			spacing: 10
			visible: !page.selectionMode

			Image {
				source: Qt.resolvedUrl("images/logo.svg")
				sourceSize: Qt.size(34, 34)
				width: 34; height: 34
			}
			AppText {
				Layout.fillWidth: true
				text: "AruniControl"
				style: "heading"
				wrapMode: Text.NoWrap
			}
			Rectangle {
				visible: App.vpn.hasConfig
				implicitHeight: 32
				implicitWidth: vpnRow.implicitWidth + 20
				radius: 16
				color: App.vpn.state === "on" ? Theme.successSoft : Theme.surfaceAlt
				Row {
					id: vpnRow
					anchors.centerIn: parent
					spacing: 4
					Icon {
						name: App.vpn.state === "on" ? "vpn_lock_fill" : "vpn_lock"
						size: 16
						color: App.vpn.state === "on" ? Theme.success : Theme.textMuted
						anchors.verticalCenter: parent.verticalCenter
					}
					Text {
						text: App.vpn.state === "on" ? "VPN" : (App.vpn.state === "connecting" ? "VPN…" : qsTr("VPN mati"))
						color: App.vpn.state === "on" ? Theme.success : Theme.textMuted
						font.family: Theme.fontFamily
						font.pixelSize: 12
						font.weight: Font.Bold
						anchors.verticalCenter: parent.verticalCenter
					}
				}
				TapHandler { onTapped: window.openRemoteAccess() }
			}
			IconButton {
				iconName: "refresh"
				tooltip: qsTr("Muat ulang")
				onClicked: {
					App.refreshComputers()
					window.toast(qsTr("Memindai ulang jaringan…"), "info")
				}
			}
			IconButton {
				iconName: "settings"
				tooltip: qsTr("Pengaturan")
				onClicked: window.openSettings()
			}
		}

		// selection bar
		RowLayout {
			anchors.fill: parent
			anchors.topMargin: window.safeTop
			anchors.leftMargin: 6 + window.safeLeft
			anchors.rightMargin: 8 + window.safeRight
			spacing: 6
			visible: page.selectionMode

			IconButton { iconName: "close"; onClicked: page.computers.clearSelection() }
			AppText {
				Layout.fillWidth: true
				text: qsTr("%1 dipilih").arg(page.computers.selectedCount)
				style: "heading"
				wrapMode: Text.NoWrap
			}
			AppButton {
				variant: "ghost"
				compact: true
				text: page.computers.selectedCount === page.computers.count ? qsTr("Batal pilih") : qsTr("Pilih semua")
				onClicked: page.computers.selectedCount === page.computers.count ? page.computers.clearSelection()
																				   : page.computers.selectAll()
			}
		}
	}

	// ---------------------------------------------------------------- grid
	GridView {
		id: grid

		readonly property int minCell: [150, 190, 280][Theme.prefs.gridSize]
		readonly property real available: width - leftMargin - rightMargin
		readonly property int columns: Math.max(1, Math.floor(available / minCell))
		property bool refreshArmed: false

		anchors.fill: parent
		leftMargin: 10 + window.safeLeft
		rightMargin: 10 + window.safeRight
		bottomMargin: 110 + window.safeBottom
		cellWidth: Math.floor(available / columns)
		cellHeight: Math.round((cellWidth - 24) * 10 / 16) + 74
		model: page.computers
		clip: false
		boundsBehavior: Flickable.DragOverBounds
		cacheBuffer: 400

		add: Transition { NumberAnimation { properties: "opacity,scale"; from: 0.6; to: 1; duration: Theme.normal; easing.type: Easing.OutCubic } }
		displaced: Transition { NumberAnimation { properties: "x,y"; duration: Theme.normal; easing.type: Easing.OutCubic } }

		// pull to refresh
		onContentYChanged: if (dragging && contentY < originY - 90) refreshArmed = true
		onDraggingChanged: if (!dragging && refreshArmed) {
			refreshArmed = false
			App.refreshComputers()
			window.toast(qsTr("Memindai ulang jaringan…"), "info")
		}

		header: ColumnLayout {
			width: grid.available
			spacing: 12

			// title + stats
			ColumnLayout {
				Layout.fillWidth: true
				Layout.leftMargin: 8
				Layout.rightMargin: 8
				Layout.topMargin: 4
				spacing: 4
				AppText {
					Layout.fillWidth: true
					text: page.computers.location.length > 0 ? page.computers.location : qsTr("Semua komputer")
					style: "title"
					font.pixelSize: 26
					elide: Text.ElideRight
					wrapMode: Text.NoWrap
				}
				RowLayout {
					spacing: 6
					Rectangle { width: 8; height: 8; radius: 4; color: page.computers.onlineCount > 0 ? Theme.success : Theme.textFaint }
					AppText {
						text: qsTr("%1 online dari %2 komputer").arg(page.computers.onlineCount).arg(page.computers.totalCount)
						style: "label"
						muted: true
						font.weight: Font.Medium
					}
				}
			}

			// rooms
			ListView {
				Layout.fillWidth: true
				Layout.preferredHeight: 36
				visible: page.computers.locations.length > 1
				orientation: ListView.Horizontal
				spacing: 8
				leftMargin: 8
				rightMargin: 8
				clip: true
				boundsBehavior: Flickable.StopAtBounds
				model: [""].concat(page.computers.locations)
				delegate: Chip {
					required property string modelData
					text: modelData.length > 0 ? modelData : qsTr("Semua ruangan")
					iconName: modelData.length > 0 ? "meeting_room" : ""
					selected: page.computers.location === modelData
					onClicked: page.computers.location = modelData
				}
			}

			// search + filter
			RowLayout {
				Layout.fillWidth: true
				Layout.leftMargin: 8
				Layout.rightMargin: 8
				spacing: 8
				InputField {
					id: search
					Layout.fillWidth: true
					iconName: "search"
					placeholderText: qsTr("Cari komputer atau pengguna")
					inputMethodHints: Qt.ImhNoPredictiveText
					onTextChanged: page.computers.searchText = text
				}
				IconButton {
					implicitWidth: 52; implicitHeight: 52
					iconName: "tune"
					backgroundColor: page.computers.filter !== 0 ? Theme.accentSoft : Theme.surfaceAlt
					iconColor: page.computers.filter !== 0 ? Theme.accent : Theme.text
					onClicked: filterSheet.open()
				}
			}

			// active filter hint
			RowLayout {
				visible: page.computers.filter !== 0
				Layout.leftMargin: 8
				spacing: 8
				Chip {
					selected: true
					iconName: "close"
					text: page.computers.filter === 1 ? qsTr("Hanya online") : qsTr("Ada pengguna")
					onClicked: page.computers.filter = 0
				}
			}

			// authentication warning
			Rectangle {
				visible: !App.authenticated
				Layout.fillWidth: true
				Layout.leftMargin: 8
				Layout.rightMargin: 8
				implicitHeight: authRow.implicitHeight + 24
				radius: Theme.radius
				color: Theme.accentSoft
				RowLayout {
					id: authRow
					anchors.fill: parent
					anchors.margins: 12
					spacing: 12
					Icon { name: "key"; color: Theme.accent; size: 24 }
					ColumnLayout {
						Layout.fillWidth: true
						spacing: 0
						AppText { Layout.fillWidth: true; text: qsTr("Akses belum diatur"); style: "label" }
						AppText { Layout.fillWidth: true; text: qsTr("Komputer akan menolak koneksi sampai kunci akses atau akun dipasang."); style: "caption"; muted: true }
					}
					AppButton { compact: true; text: qsTr("Atur"); onClicked: window.openAuth(true) }
				}
				TapHandler { onTapped: window.openAuth(true) }
			}

			Item { Layout.preferredHeight: 2 }
		}

		delegate: ComputerCard {
			width: grid.cellWidth
			height: grid.cellHeight
			selectionMode: page.selectionMode
			onTapped: {
				if (page.selectionMode)
					page.computers.toggleSelected(uid)
				else
					window.openComputer(uid)
			}
			onLongPressed: page.computers.toggleSelected(uid)
		}

		// pull-to-refresh indicator
		Rectangle {
			parent: grid
			anchors.horizontalCenter: parent.horizontalCenter
			y: Math.min(40, (grid.originY - grid.contentY) / 2) - 20
			width: 40; height: 40; radius: 20
			color: Theme.surface
			border.color: Theme.border
			visible: grid.contentY < grid.originY - 10
			opacity: Math.min(1, (grid.originY - grid.contentY) / 90)
			Icon {
				anchors.centerIn: parent
				name: "refresh"
				color: grid.refreshArmed ? Theme.accent : Theme.textMuted
				rotation: (grid.originY - grid.contentY) * 3
			}
		}
	}

	// empty state
	EmptyState {
		visible: page.computers.count === 0
		anchors.centerIn: parent
		anchors.verticalCenterOffset: 60
		width: Math.min(parent.width - 48, 420)
		busy: page.computers.totalCount === 0
		iconName: page.computers.totalCount === 0 ? "wifi_find" : "search"
		title: page.computers.totalCount === 0 ? qsTr("Mencari komputer…") : qsTr("Tidak ada yang cocok")
		text: page.computers.totalCount === 0
			  ? qsTr("Pastikan HP terhubung ke Wi-Fi yang sama dengan komputer dan AruniControl sudah terpasang di komputer.") +
				(App.networkAddress.length > 0 ? "\n" + qsTr("Alamat HP ini: %1").arg(App.networkAddress) : "")
			  : qsTr("Coba kata kunci lain atau hapus filter.")

		AppButton {
			visible: page.computers.totalCount === 0
			variant: "filled"
			text: App.gateways.sites.length > 0 ? qsTr("Lihat lokasi lewat internet") : qsTr("Pakai paket data? Pindai QR gateway")
			iconName: "qr_code_scanner"
			onClicked: window.openRemoteAccess()
		}
		AppButton {
			visible: page.computers.totalCount === 0
			variant: "tonal"
			text: qsTr("Tambah komputer manual")
			iconName: "add"
			onClicked: window.openRooms()
		}
	}

	// ---------------------------------------------------------------- quick action bar
	Rectangle {
		id: actionBar
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.bottom: parent.bottom
		anchors.bottomMargin: 16 + window.safeBottom
		width: Math.min(parent.width - 24, 520)
		height: 76
		radius: 28
		color: Theme.dark ? "#2A2521" : "#FFFFFF"
		border.width: Theme.dark ? 0 : 1
		border.color: Theme.border
		visible: page.computers.count > 0

		// soft shadow
		Rectangle {
			z: -1
			anchors.fill: parent
			anchors.topMargin: 6
			anchors.leftMargin: 4
			anchors.rightMargin: 4
			anchors.bottomMargin: -6
			radius: parent.radius
			color: "#000000"
			opacity: Theme.dark ? 0.35 : 0.08
		}

		RowLayout {
			anchors.fill: parent
			anchors.margins: 8
			spacing: 4

			Repeater {
				model: [
					{ icon: "lock", label: qsTr("Kunci"), action: "lock" },
					{ icon: "lock_open", label: qsTr("Buka"), action: "unlock" },
					{ icon: "chat", label: qsTr("Pesan"), action: "message" }
				]
				delegate: AbstractButton {
					id: quick
					required property var modelData
					Layout.fillWidth: true
					Layout.fillHeight: true
					background: Rectangle {
						radius: 20
						color: quick.pressed ? Theme.surfaceAlt : "transparent"
					}
					contentItem: ColumnLayout {
						spacing: 2
						Icon { Layout.alignment: Qt.AlignHCenter; name: quick.modelData.icon; size: 24 }
						Text {
							Layout.alignment: Qt.AlignHCenter
							text: quick.modelData.label
							color: Theme.text
							font.family: Theme.fontFamily
							font.pixelSize: 12
							font.weight: Font.DemiBold
						}
					}
					onClicked: {
						if (modelData.action === "lock")
							App.lockScreens(true, page.targets())
						else if (modelData.action === "unlock")
							App.lockScreens(false, page.targets())
						else {
							messageSheet.targetUids = page.targets()
							messageSheet.targetLabel = page.targetLabel
							messageSheet.open()
						}
					}
				}
			}

			AbstractButton {
				id: moreButton
				Layout.fillHeight: true
				Layout.preferredWidth: Math.max(110, moreRow.implicitWidth + 28)
				background: Rectangle {
					radius: 22
					color: moreButton.pressed ? Theme.accentPressed : Theme.accent
				}
				contentItem: ColumnLayout {
					id: moreRow
					spacing: 0
					Icon { Layout.alignment: Qt.AlignHCenter; name: "bolt"; size: 24; color: Theme.textOnAccent }
					Text {
						Layout.alignment: Qt.AlignHCenter
						text: page.targetLabel
						color: Theme.textOnAccent
						font.family: Theme.fontFamily
						font.pixelSize: 12
						font.weight: Font.Bold
					}
				}
				onClicked: page.openActions()
			}
		}
	}

	// ---------------------------------------------------------------- sheets
	ActionSheet {
		id: actionSheet
		onMessageRequested: (uids, label) => {
			messageSheet.targetUids = uids
			messageSheet.targetLabel = label
			messageSheet.open()
		}
	}

	MessageSheet { id: messageSheet }

	Sheet {
		id: filterSheet
		title: qsTr("Tampilkan")
		iconName: "filter_list"

		Repeater {
			model: [
				{ label: qsTr("Semua komputer"), icon: "desktop_windows", value: 0 },
				{ label: qsTr("Hanya yang online"), icon: "wifi", value: 1 },
				{ label: qsTr("Ada pengguna yang login"), icon: "person", value: 2 }
			]
			delegate: ListRow {
				required property var modelData
				Layout.fillWidth: true
				Layout.leftMargin: -Theme.padLarge
				Layout.rightMargin: -Theme.padLarge
				leftPadding: Theme.padLarge
				rightPadding: Theme.padLarge
				iconName: modelData.icon
				title: modelData.label
				chevron: false
				Icon {
					visible: page.computers.filter === modelData.value
					name: "check_circle_fill"
					color: Theme.accent
				}
				onClicked: {
					page.computers.filter = modelData.value
					filterSheet.close()
				}
			}
		}

		AppText { text: qsTr("Ukuran tampilan"); style: "label"; muted: true; Layout.topMargin: 8 }
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
	}
}
