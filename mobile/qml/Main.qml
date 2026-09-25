import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window

ApplicationWindow {
	id: window

	// insets of system bars/cutouts; pages draw under them and pad themselves
	readonly property real safeTop: SafeArea.margins.top
	readonly property real safeBottom: SafeArea.margins.bottom
	readonly property real safeLeft: SafeArea.margins.left
	readonly property real safeRight: SafeArea.margins.right
	readonly property bool wide: width >= 720

	width: 420
	height: 880
	visible: true
	title: "AruniControl"
	color: Theme.background
	flags: Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint

	font.family: Theme.fontFamily
	font.pixelSize: Theme.fontBody

	// draw edge-to-edge; every page pads itself with safeTop/safeBottom
	topPadding: 0
	bottomPadding: 0
	leftPadding: 0
	rightPadding: 0

	palette.window: Theme.background
	palette.base: Theme.surface
	palette.text: Theme.text
	palette.windowText: Theme.text
	palette.buttonText: Theme.text
	palette.highlight: Theme.accent
	palette.highlightedText: Theme.textOnAccent
	palette.placeholderText: Theme.textFaint

	function toast(message, kind) { toastItem.show(message, kind) }

	function openComputer(uid) {
		stack.push(Qt.resolvedUrl("RemotePage.qml"), { computerUid: uid })
	}

	function openSettings() { stack.push(Qt.resolvedUrl("SettingsPage.qml")) }
	function openAuth(fromSettings) { stack.push(Qt.resolvedUrl("AuthPage.qml"), { fromSettings: !!fromSettings }) }
	function openRooms() { stack.push(Qt.resolvedUrl("RoomsPage.qml")) }
	function openScreenshots() { stack.push(Qt.resolvedUrl("ScreenshotsPage.qml")) }
	function openRemoteAccess() { stack.push(Qt.resolvedUrl("RemoteAccessPage.qml")) }
	function openCollected() { stack.push(Qt.resolvedUrl("CollectedFilesPage.qml")) }

	function goHome() {
		stack.replace(null, Qt.resolvedUrl("HomePage.qml"))
	}

	StackView {
		id: stack
		anchors.fill: parent

		// (a bound initialItem is not picked up reliably, so push explicitly)
		Component.onCompleted: push(App.authenticated || Theme.prefs.onboardingDone ? Qt.resolvedUrl("HomePage.qml")
																					 : Qt.resolvedUrl("WelcomePage.qml"),
									{}, StackView.Immediate)

		pushEnter: Transition {
			ParallelAnimation {
				NumberAnimation { property: "x"; from: stack.width * 0.25; to: 0; duration: Theme.normal; easing.type: Easing.OutCubic }
				NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.normal; easing.type: Easing.OutCubic }
			}
		}
		pushExit: Transition {
			NumberAnimation { property: "opacity"; from: 1; to: 0.4; duration: Theme.normal }
		}
		popEnter: Transition {
			NumberAnimation { property: "opacity"; from: 0.4; to: 1; duration: Theme.normal }
		}
		popExit: Transition {
			ParallelAnimation {
				NumberAnimation { property: "x"; from: 0; to: stack.width * 0.25; duration: Theme.normal; easing.type: Easing.InCubic }
				NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.normal; easing.type: Easing.InCubic }
			}
		}
		replaceEnter: Transition {
			NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.slow; easing.type: Easing.OutCubic }
		}
		replaceExit: Transition {
			NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.normal }
		}
	}

	// development hook (desktop builds): AC_PAGE=auth|settings|rooms|shots|actions|message|remote|welcome
	Timer {
		running: App.devOption("AC_PAGE").length > 0
		interval: Number(App.devOption("AC_DELAY") || 2500)
		onTriggered: {
			switch (App.devOption("AC_PAGE")) {
			case "welcome": stack.push(Qt.resolvedUrl("WelcomePage.qml")); break
			case "auth": window.openAuth(true); break
			case "settings": window.openSettings(); break
			case "rooms": window.openRooms(); break
			case "shots": window.openScreenshots(); break
			case "vpn": window.openRemoteAccess(); break
			case "actions": stack.currentItem.openActions(); break
			case "remote":
				if (App.computers.count > 0)
					window.openComputer(App.computers.uidAt(0))
				break
			}
		}
	}

	// Android back button / gesture
	onClosing: (close) => {
		if (stack.depth > 1) {
			close.accepted = false
			stack.pop()
		}
	}

	Toast {
		id: toastItem
		bottomInset: window.safeBottom
	}

	Connections {
		target: App
		function onNotify(message, kind) { window.toast(message, kind) }
	}
}
