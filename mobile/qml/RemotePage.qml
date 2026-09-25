import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Window
import AruniControl

Page {
	id: page

	property string computerUid
	property bool controlMode: false
	property bool chromeVisible: true

	// zoom & pan of the remote screen inside the stage
	property real zoom: 1
	property real panX: 0
	property real panY: 0

	readonly property size fb: view.framebufferSize
	readonly property real fitScale: fb.width > 0 && fb.height > 0 ? Math.min(stage.width / fb.width, stage.height / fb.height) : 1
	readonly property real viewWidth: fb.width * fitScale * zoom
	readonly property real viewHeight: fb.height * fitScale * zoom
	readonly property string computerName: App.computers.nameOf(computerUid)

	function clampPan() {
		const maxX = Math.max(0, (viewWidth - stage.width) / 2)
		const maxY = Math.max(0, (viewHeight - stage.height) / 2)
		panX = Math.max(-maxX, Math.min(maxX, panX))
		panY = Math.max(-maxY, Math.min(maxY, panY))
	}

	function zoomAt(newZoom, px, py) {
		// keep the content point under (px, py) in place
		const z = Math.max(1, Math.min(6, newZoom))
		const cx = px - stage.width / 2
		const cy = py - stage.height / 2
		panX = cx - (cx - panX) * z / zoom
		panY = cy - (cy - panY) * z / zoom
		zoom = z
		clampPan()
	}

	function resetZoom() {
		zoomAnimation.stop()
		zoom = 1; panX = 0; panY = 0
	}

	function showChrome() {
		chromeVisible = true
		hideTimer.restart()
	}

	function takeScreenshot() {
		const path = view.saveScreenshot(App.screenshotDirectory)
		if (path.length > 0) {
			shotSheet.path = path
			shotSheet.open()
		} else {
			window.toast(qsTr("Layar belum tersedia"), "error")
		}
	}

	onControlModeChanged: {
		showChrome()
		if (controlMode && !Theme.prefs.remoteHintShown)
			hint.visible = true
	}

	Component.onCompleted: showChrome()
	onViewWidthChanged: clampPan()
	onViewHeightChanged: clampPan()

	background: Rectangle { color: "#000000" }

	Timer {
		id: hideTimer
		interval: 3500
		onTriggered: if (!page.controlMode) page.chromeVisible = false
	}

	// ------------------------------------------------------------ stage
	Item {
		id: stage
		anchors.fill: parent
		anchors.topMargin: page.controlMode ? topBar.height : 0
		anchors.bottomMargin: page.controlMode ? keyBar.height : 0
		clip: true

		Behavior on anchors.topMargin { NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic } }
		Behavior on anchors.bottomMargin { NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic } }

		RemoteViewItem {
			id: view
			computerUid: page.computerUid
			controlMode: page.controlMode
			width: page.viewWidth
			height: page.viewHeight
			x: (stage.width - width) / 2 + page.panX
			y: (stage.height - height) / 2 + page.panY

			// --- control mode: touch acts as the mouse
			TapHandler {
				enabled: page.controlMode
				acceptedButtons: Qt.LeftButton | Qt.RightButton
				onTapped: (eventPoint, button) => {
					view.click(eventPoint.position.x, eventPoint.position.y, button === Qt.RightButton ? 4 : 1, 1)
				}
				onLongPressed: view.click(point.position.x, point.position.y, 4, 1)
			}

			DragHandler {
				id: controlDrag
				enabled: page.controlMode
				target: null
				minimumPointCount: 1
				maximumPointCount: 1
				dragThreshold: 6
				onActiveChanged: {
					if (active)
						view.pressButton(centroid.pressPosition.x, centroid.pressPosition.y, 1)
					else
						view.releaseButton(view.pointer.x, view.pointer.y, 1)
				}
				onCentroidChanged: if (active) view.movePointer(centroid.position.x, centroid.position.y)
			}

			HoverHandler {
				enabled: page.controlMode
				acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
				onPointChanged: if (hovered) view.movePointer(point.position.x, point.position.y)
			}

			WheelHandler {
				enabled: page.controlMode
				target: null
				onWheel: (event) => view.scroll(event.x, event.y, event.angleDelta.y > 0 ? 1 : -1)
			}
		}

		// --- two fingers: zoom and pan, in both modes
		PinchHandler {
			id: pinch
			target: null
			minimumPointCount: 2
			maximumPointCount: 2
			property real startZoom: 1
			property real startPanX: 0
			property real startPanY: 0
			property point startCentroid
			onActiveChanged: if (active) {
				zoomAnimation.stop()
				startZoom = page.zoom
				startPanX = page.panX
				startPanY = page.panY
				startCentroid = centroid.position
			}
			onActiveScaleChanged: update()
			onCentroidChanged: update()
			function update() {
				if (!active)
					return
				const z = Math.max(1, Math.min(6, startZoom * activeScale))
				const sx = startCentroid.x - stage.width / 2
				const sy = startCentroid.y - stage.height / 2
				const cx = centroid.position.x - stage.width / 2
				const cy = centroid.position.y - stage.height / 2
				page.panX = cx - (sx - startPanX) * z / startZoom
				page.panY = cy - (sy - startPanY) * z / startZoom
				page.zoom = z
				page.clampPan()
			}
		}

		// --- view mode: one finger pans a zoomed screen, taps toggle the chrome
		DragHandler {
			enabled: !page.controlMode && page.zoom > 1.01
			target: null
			maximumPointCount: 1
			property real startX
			property real startY
			onActiveChanged: if (active) { startX = page.panX; startY = page.panY }
			onTranslationChanged: {
				page.panX = startX + translation.x
				page.panY = startY + translation.y
				page.clampPan()
			}
		}

		TapHandler {
			enabled: !page.controlMode
			onTapped: page.chromeVisible ? (page.chromeVisible = false) : page.showChrome()
			onDoubleTapped: (eventPoint) => {
				zoomAnimation.stop()
				zoomAnimation.px = eventPoint.position.x
				zoomAnimation.py = eventPoint.position.y
				zoomAnimation.from = page.zoom
				zoomAnimation.to = page.zoom > 1.5 ? 1 : 2.5
				zoomAnimation.start()
			}
		}

		NumberAnimation {
			id: zoomAnimation
			property real px
			property real py
			property real value: 1
			target: zoomAnimation
			property: "value"
			duration: Theme.normal
			easing.type: Easing.OutCubic
			onValueChanged: page.zoomAt(value, px, py)
		}
	}

	// ------------------------------------------------------------ connecting / error overlay
	ColumnLayout {
		anchors.centerIn: parent
		width: Math.min(parent.width - 48, 360)
		visible: !view.hasImage
		spacing: 14

		BusyIndicator {
			Layout.alignment: Qt.AlignHCenter
			running: view.status === "connecting" || view.status === "offline"
			visible: running
		}
		Icon {
			Layout.alignment: Qt.AlignHCenter
			visible: view.status === "denied" || view.status === "noservice"
			name: view.status === "denied" ? "key" : "error"
			color: Theme.danger
			size: 44
		}
		Text {
			Layout.fillWidth: true
			horizontalAlignment: Text.AlignHCenter
			wrapMode: Text.Wrap
			color: "#F4EFE9"
			font.family: Theme.fontFamily
			font.pixelSize: Theme.fontBody
			font.weight: Font.DemiBold
			text: {
				switch (view.status) {
				case "denied": return qsTr("Akses ditolak oleh %1.\nPeriksa kunci akses atau akun di Pengaturan.").arg(page.computerName)
				case "noservice": return qsTr("AruniControl belum berjalan di %1.").arg(page.computerName)
				default: return qsTr("Menghubungkan ke %1…").arg(page.computerName)
				}
			}
		}
	}

	// ------------------------------------------------------------ top bar
	Rectangle {
		id: topBar
		anchors.left: parent.left
		anchors.right: parent.right
		y: page.chromeVisible || page.controlMode ? 0 : -height
		height: 60 + window.safeTop
		color: page.controlMode ? "#1A1714" : "#CC000000"
		Behavior on y { NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic } }

		RowLayout {
			anchors.fill: parent
			anchors.topMargin: window.safeTop
			anchors.leftMargin: 4 + window.safeLeft
			anchors.rightMargin: 8 + window.safeRight
			spacing: 4

			IconButton {
				iconName: "arrow_back"
				iconColor: "#FFFFFF"
				onClicked: page.StackView.view.pop()
			}
			ColumnLayout {
				Layout.fillWidth: true
				spacing: 0
				Text {
					Layout.fillWidth: true
					text: page.computerName
					color: "#FFFFFF"
					font.family: Theme.fontFamily
					font.pixelSize: Theme.fontBody
					font.weight: Font.Bold
					elide: Text.ElideRight
				}
				RowLayout {
					spacing: 5
					Rectangle { width: 7; height: 7; radius: 4; color: Theme.statusColor(view.status) }
					Text {
						text: view.status === "online" ? (page.controlMode ? qsTr("Mengendalikan") : qsTr("Live")) : qsTr("Tidak terhubung")
						color: "#C9C1B9"
						font.family: Theme.fontFamily
						font.pixelSize: 11
						font.weight: Font.Medium
					}
				}
			}

			// view / control switch
			Rectangle {
				implicitWidth: modeRow.implicitWidth + 8
				implicitHeight: 40
				radius: 20
				color: "#33FFFFFF"
				Row {
					id: modeRow
					anchors.centerIn: parent
					Repeater {
						model: [ { label: qsTr("Lihat"), icon: "visibility", control: false }, { label: qsTr("Kontrol"), icon: "touch_app", control: true } ]
						delegate: AbstractButton {
							id: modeButton
							required property var modelData
							readonly property bool active: page.controlMode === modelData.control
							implicitHeight: 32
							implicitWidth: modeLabel.implicitWidth + 44
							background: Rectangle {
								radius: 16
								color: modeButton.active ? (modeButton.modelData.control ? Theme.accent : "#FFFFFF") : "transparent"
								Behavior on color { ColorAnimation { duration: Theme.fast } }
							}
							contentItem: Row {
								spacing: 4
								leftPadding: 10
								Icon {
									name: modeButton.modelData.icon
									size: 18
									color: modeButton.active ? (modeButton.modelData.control ? "#FFFFFF" : "#1E1B18") : "#FFFFFF"
									anchors.verticalCenter: parent.verticalCenter
								}
								Text {
									id: modeLabel
									text: modeButton.modelData.label
									color: modeButton.active ? (modeButton.modelData.control ? "#FFFFFF" : "#1E1B18") : "#FFFFFF"
									font.family: Theme.fontFamily
									font.pixelSize: 13
									font.weight: Font.Bold
									anchors.verticalCenter: parent.verticalCenter
								}
							}
							onClicked: page.controlMode = modelData.control
						}
					}
				}
			}

			IconButton {
				iconName: "more_vert"
				iconColor: "#FFFFFF"
				onClicked: moreSheet.open()
			}
		}
	}

	// ------------------------------------------------------------ view-mode floating buttons
	Row {
		anchors.right: parent.right
		anchors.bottom: parent.bottom
		anchors.rightMargin: 16 + window.safeRight
		anchors.bottomMargin: 20 + window.safeBottom
		spacing: 10
		visible: !page.controlMode
		opacity: page.chromeVisible ? 1 : 0
		Behavior on opacity { NumberAnimation { duration: Theme.normal } }

		IconButton {
			visible: page.zoom > 1.01
			implicitWidth: 52; implicitHeight: 52
			iconName: "fullscreen_exit"
			iconColor: "#FFFFFF"
			backgroundColor: "#99000000"
			onClicked: page.resetZoom()
		}
		IconButton {
			implicitWidth: 52; implicitHeight: 52
			iconName: "photo_camera"
			iconColor: "#FFFFFF"
			backgroundColor: "#99000000"
			onClicked: page.takeScreenshot()
		}
		AppButton {
			text: qsTr("Kendalikan")
			iconName: "touch_app"
			onClicked: page.controlMode = true
		}
	}

	// ------------------------------------------------------------ control-mode scroll strip
	Rectangle {
		id: scrollStrip
		visible: page.controlMode
		anchors.right: parent.right
		anchors.rightMargin: 8 + window.safeRight
		anchors.verticalCenter: stage.verticalCenter
		width: 40
		height: Math.min(220, stage.height * 0.5)
		radius: 20
		color: stripDrag.active ? "#CCF2812F" : "#80000000"
		border.width: 1
		border.color: "#40FFFFFF"

		Column {
			anchors.centerIn: parent
			spacing: 4
			Icon { name: "arrow_upward"; color: "#FFFFFF"; size: 18; anchors.horizontalCenter: parent.horizontalCenter }
			Icon { name: "mouse"; color: "#FFFFFF"; size: 18; opacity: 0.8; anchors.horizontalCenter: parent.horizontalCenter }
			Icon { name: "arrow_downward"; color: "#FFFFFF"; size: 18; anchors.horizontalCenter: parent.horizontalCenter }
		}

		DragHandler {
			id: stripDrag
			target: null
			xAxis.enabled: false
			property real consumed: 0
			onActiveChanged: consumed = 0
			onTranslationChanged: {
				const steps = Math.trunc((translation.y - consumed) / 24)
				if (steps !== 0) {
					consumed += steps * 24
					// dragging down scrolls the content down, like a touchscreen
					view.scroll(view.pointer.x, view.pointer.y, steps)
				}
			}
		}
		TapHandler {
			onTapped: (eventPoint) => view.scroll(view.pointer.x, view.pointer.y, eventPoint.position.y < scrollStrip.height / 2 ? 2 : -2)
		}
	}

	// ------------------------------------------------------------ control-mode key bar
	Rectangle {
		id: keyBar
		anchors.left: parent.left
		anchors.right: parent.right
		anchors.bottom: parent.bottom
		height: 64 + window.safeBottom
		color: "#1A1714"
		visible: page.controlMode

		ListView {
			anchors.fill: parent
			anchors.bottomMargin: window.safeBottom
			anchors.leftMargin: window.safeLeft
			anchors.rightMargin: window.safeRight
			orientation: ListView.Horizontal
			leftMargin: 10
			rightMargin: 10
			spacing: 8
			clip: true
			boundsBehavior: Flickable.StopAtBounds
			model: [
				{ label: "", icon: "keyboard", key: "__keyboard" },
				{ label: "Esc", key: "escape" },
				{ label: "Tab", key: "tab" },
				{ label: "Enter", icon: "keyboard_return", key: "enter" },
				{ label: "", icon: "backspace", key: "backspace" },
				{ label: "Ctrl+Alt+Del", key: "ctrl+alt+del", shortcut: true },
				{ label: "Win", icon: "grid_view", key: "win", shortcut: true },
				{ label: "Alt+Tab", key: "alt+tab", shortcut: true },
				{ label: "Alt+F4", key: "alt+f4", shortcut: true },
				{ label: "Ctrl+C", key: "ctrl+c" },
				{ label: "Ctrl+V", key: "ctrl+v" },
				{ label: "Ctrl+Z", key: "ctrl+z" },
				{ label: "", icon: "arrow_back", key: "left" },
				{ label: "", icon: "arrow_upward", key: "up" },
				{ label: "", icon: "arrow_downward", key: "down" },
				{ label: "", icon: "arrow_forward", key: "right" },
				{ label: "F5", key: "f5" },
				{ label: "", icon: "photo_camera", key: "__screenshot" }
			]
			delegate: AbstractButton {
				id: keyButton
				required property var modelData
				anchors.verticalCenter: parent ? parent.verticalCenter : undefined
				implicitHeight: 44
				implicitWidth: Math.max(48, keyRow.implicitWidth + 24)
				readonly property bool isKeyboard: modelData.key === "__keyboard"
				background: Rectangle {
					radius: 12
					color: keyButton.isKeyboard && keyInput.activeFocus ? Theme.accent : (keyButton.pressed ? "#4DFFFFFF" : "#26FFFFFF")
				}
				contentItem: Item {
					Row {
						id: keyRow
						anchors.centerIn: parent
						spacing: 4
						Icon {
							visible: !!keyButton.modelData.icon
							name: keyButton.modelData.icon || ""
							color: "#FFFFFF"
							size: 20
							anchors.verticalCenter: parent.verticalCenter
						}
						Text {
							visible: keyButton.modelData.label.length > 0
							text: keyButton.modelData.label
							color: "#FFFFFF"
							font.family: Theme.fontFamily
							font.pixelSize: 13
							font.weight: Font.DemiBold
							anchors.verticalCenter: parent.verticalCenter
						}
					}
				}
				onClicked: {
					if (modelData.key === "__keyboard") {
						if (keyInput.activeFocus) {
							keyInput.focus = false
							Qt.inputMethod.hide()
						} else {
							keyInput.forceActiveFocus()
							Qt.inputMethod.show()
						}
					} else if (modelData.key === "__screenshot") {
						page.takeScreenshot()
					} else if (modelData.shortcut) {
						view.sendShortcut(modelData.key)
					} else {
						view.pressKey(modelData.key)
					}
				}
			}
		}
	}

	// hidden text input receiving the soft keyboard; its content is kept at a
	// sentinel so that both typed characters and backspaces can be detected
	TextInput {
		id: keyInput
		readonly property string sentinel: "  "
		x: -1000
		width: 10
		text: sentinel
		inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhSensitiveData
		onTextEdited: {
			if (text.length < sentinel.length) {
				for (let i = text.length; i < sentinel.length; ++i)
					view.pressKey("backspace")
			} else if (text.length > sentinel.length) {
				view.typeText(text.substring(sentinel.length))
			}
			text = sentinel
			cursorPosition = sentinel.length
		}
		onAccepted: view.pressKey("enter")
		Keys.onPressed: (event) => {
			// hardware keyboards and keys the IME passes through
			const special = { [Qt.Key_Backspace]: "backspace", [Qt.Key_Escape]: "escape", [Qt.Key_Tab]: "tab",
							  [Qt.Key_Left]: "left", [Qt.Key_Right]: "right", [Qt.Key_Up]: "up", [Qt.Key_Down]: "down",
							  [Qt.Key_Delete]: "delete", [Qt.Key_Home]: "home", [Qt.Key_End]: "end" }
			if (special[event.key] !== undefined) {
				view.pressKey(special[event.key])
				event.accepted = true
			}
		}
	}

	// ------------------------------------------------------------ first-use gesture hint
	Rectangle {
		id: hint
		visible: false
		anchors.fill: parent
		color: "#CC000000"
		z: 100

		ColumnLayout {
			anchors.centerIn: parent
			width: Math.min(parent.width - 48, 380)
			spacing: 14

			Text {
				Layout.fillWidth: true
				text: qsTr("Cara mengendalikan")
				color: "#FFFFFF"
				font.family: Theme.fontFamily
				font.pixelSize: Theme.fontTitle
				font.weight: Font.Bold
			}
			Repeater {
				model: [
					{ icon: "touch_app", text: qsTr("Ketuk untuk klik, ketuk dua kali untuk klik ganda") },
					{ icon: "pan_tool", text: qsTr("Tahan sebentar untuk klik kanan") },
					{ icon: "swipe", text: qsTr("Geser satu jari untuk menyeret") },
					{ icon: "pinch", text: qsTr("Dua jari untuk memperbesar dan menggeser layar") },
					{ icon: "mouse", text: qsTr("Geser strip di kanan untuk scroll") },
					{ icon: "keyboard", text: qsTr("Tombol keyboard di bawah untuk mengetik") }
				]
				delegate: RowLayout {
					required property var modelData
					Layout.fillWidth: true
					spacing: 12
					Rectangle {
						width: 40; height: 40; radius: 12
						color: "#33F2812F"
						Icon { anchors.centerIn: parent; name: modelData.icon; color: Theme.accent; size: 22 }
					}
					Text {
						Layout.fillWidth: true
						text: modelData.text
						color: "#F4EFE9"
						wrapMode: Text.Wrap
						font.family: Theme.fontFamily
						font.pixelSize: Theme.fontLabel
					}
				}
			}
			AppButton {
				Layout.fillWidth: true
				Layout.topMargin: 8
				text: qsTr("Mengerti")
				onClicked: {
					hint.visible = false
					Theme.prefs.remoteHintShown = true
				}
			}
		}
	}

	// ------------------------------------------------------------ sheets
	Sheet {
		id: moreSheet
		title: page.computerName
		subtitle: qsTr("Aksi untuk komputer ini")
		iconName: "desktop_windows_fill"

		Repeater {
			model: [
				{ key: "lock", icon: "lock", label: qsTr("Kunci layar") },
				{ key: "unlock", icon: "lock_open", label: qsTr("Buka kunci layar") },
				{ key: "message", icon: "chat", label: qsTr("Kirim pesan") },
				{ key: "share", icon: "screen_share", label: qsTr("Tampilkan layar ini ke semua") },
				{ key: "inputLock", icon: "keyboard_off", label: qsTr("Kunci keyboard & mouse") },
				{ key: "inputUnlock", icon: "keyboard", label: qsTr("Buka keyboard & mouse") },
				{ key: "screenshot", icon: "photo_camera", label: qsTr("Tangkap layar") },
				{ key: "fit", icon: "fullscreen_exit", label: qsTr("Tampilkan layar penuh (reset zoom)") },
				{ key: "reboot", icon: "restart_alt", label: qsTr("Mulai ulang"), danger: true },
				{ key: "off", icon: "power_settings_new", label: qsTr("Matikan"), danger: true }
			]
			delegate: ListRow {
				required property var modelData
				Layout.fillWidth: true
				Layout.leftMargin: -Theme.padLarge
				Layout.rightMargin: -Theme.padLarge
				leftPadding: Theme.padLarge
				rightPadding: Theme.padLarge
				iconName: modelData.icon
				iconColor: modelData.danger ? Theme.danger : Theme.accent
				title: modelData.label
				danger: !!modelData.danger
				chevron: false
				onClicked: {
					moreSheet.close()
					const uids = [page.computerUid]
					switch (modelData.key) {
					case "lock": App.lockScreens(true, uids); break
					case "unlock": App.lockScreens(false, uids); break
					case "message":
						messageSheet.targetUids = uids
						messageSheet.targetLabel = page.computerName
						messageSheet.open()
						break
					case "screenshot": page.takeScreenshot(); break
					case "share":
						shareSheet.sourceUid = page.computerUid
						shareSheet.open()
						break
					case "inputLock": App.lockInput(true, uids); break
					case "inputUnlock": App.lockInput(false, uids); break
					case "fit": page.resetZoom(); break
					case "reboot":
					case "off":
						confirmSheet.action = modelData.key
						confirmSheet.targetUids = uids
						confirmSheet.targetLabel = page.computerName
						confirmSheet.open()
						break
					}
				}
			}
		}
	}

	MessageSheet { id: messageSheet }
	ConfirmSheet { id: confirmSheet }
	ShareScreenSheet { id: shareSheet }

	Sheet {
		id: shotSheet
		property string path
		title: qsTr("Tangkapan layar tersimpan")
		iconName: "photo_camera"
		iconColor: Theme.success

		Image {
			Layout.fillWidth: true
			Layout.preferredHeight: width * 9 / 16
			source: shotSheet.path.length > 0 ? "file://" + shotSheet.path : ""
			fillMode: Image.PreserveAspectFit
			asynchronous: true
			cache: false
		}
		RowLayout {
			Layout.fillWidth: true
			spacing: 10
			AppButton { Layout.fillWidth: true; variant: "outline"; text: qsTr("Tutup"); onClicked: shotSheet.close() }
			AppButton {
				Layout.fillWidth: true
				text: qsTr("Bagikan")
				iconName: "share"
				onClicked: {
					App.shareFile(shotSheet.path)
					shotSheet.close()
				}
			}
		}
	}
}
