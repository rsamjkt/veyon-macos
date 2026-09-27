import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
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

	// per-computer tools (Chat, AruniVoice, application monitoring)
	function openChat(uid) {
		const current = stack.currentItem
		if (current && current.objectName === "chatPage") {
			if (current.computerUid === uid)
				return
			stack.pop(StackView.Immediate)
		}
		stack.push(Qt.resolvedUrl("ChatPage.qml"), { computerUid: uid, objectName: "chatPage" })
	}
	function openApps(uid) { stack.push(Qt.resolvedUrl("AppsPage.qml"), { computerUid: uid }) }
	function openVoice(uid) { voiceSheet.start(uid) }
	function openAccessLog(uid) { stack.push(Qt.resolvedUrl("AccessLogPage.qml"), { computerUid: uid }) }

	// tools for many computers at once (an empty uid list = all visible computers)
	function openBroadcast(uids, label) { broadcastSheet.start(uids, label) }
	function openSiteBlock(uids, label) { siteBlockSheet.start(uids, label) }
	function openExam(uids, label) { examSheet.start(uids, label) }

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

	// development hook (desktop builds): AC_PAGE=auth|settings|rooms|shots|actions|message|remote|welcome|
	// broadcast|siteblock|accesslog
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
			case "broadcast": window.openBroadcast([], qsTr("Semua komputer")); break
			case "siteblock": window.openSiteBlock([], qsTr("Semua komputer")); break
			case "exam": window.openExam([], qsTr("Semua komputer")); break
			case "accesslog":
				if (App.computers.count > 0)
					window.openAccessLog(App.computers.uidAt(0))
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

	VoiceSheet { id: voiceSheet }
	BroadcastSheet { id: broadcastSheet }
	SiteBlockSheet { id: siteBlockSheet }
	ExamSheet { id: examSheet }

	// in-app notification for chat replies that arrive while that chat is closed
	Rectangle {
		id: replyBanner
		property string uid
		property string heading
		property string body

		function show(uid, computerName, sender, text) {
			replyBanner.uid = uid
			heading = sender.length > 0 ? qsTr("%1 · %2").arg(sender).arg(computerName) : computerName
			body = text
			bannerTimer.restart()
			shown = true
		}
		property bool shown: false

		z: 1001
		anchors.horizontalCenter: parent.horizontalCenter
		width: Math.min(parent.width - 2 * Theme.pad, 520)
		height: bannerRow.implicitHeight + 24
		y: shown ? window.safeTop + 8 : -height - 8
		radius: 20
		color: Theme.surface
		border.width: 1
		border.color: Theme.border
		Behavior on y { NumberAnimation { duration: Theme.normal; easing.type: Easing.OutCubic } }

		Rectangle {
			z: -1
			anchors.fill: parent
			anchors.topMargin: 4
			anchors.bottomMargin: -6
			radius: parent.radius
			color: "#000000"
			opacity: Theme.dark ? 0.4 : 0.1
		}

		Timer { id: bannerTimer; interval: 6000; onTriggered: replyBanner.shown = false }

		RowLayout {
			id: bannerRow
			anchors.fill: parent
			anchors.margins: 12
			spacing: 12
			Rectangle {
				width: 40; height: 40; radius: 20
				color: Theme.accentSoft
				Icon { anchors.centerIn: parent; name: "forum_fill"; color: Theme.accent; size: 22 }
			}
			ColumnLayout {
				Layout.fillWidth: true
				spacing: 1
				AppText { Layout.fillWidth: true; text: replyBanner.heading; style: "label"; elide: Text.ElideRight; wrapMode: Text.NoWrap }
				AppText { Layout.fillWidth: true; text: replyBanner.body; style: "caption"; muted: true; elide: Text.ElideRight; maximumLineCount: 2 }
			}
			AppText { text: qsTr("Balas"); style: "label"; color: Theme.accent }
		}

		TapHandler {
			onTapped: {
				replyBanner.shown = false
				window.openChat(replyBanner.uid)
			}
		}
		DragHandler {
			target: null
			yAxis.enabled: true
			xAxis.enabled: false
			onActiveChanged: if (!active && translation.y < -10) replyBanner.shown = false
		}
	}

	Connections {
		target: App.chat
		function onReplyReceived(uid, computerName, sender, text) { replyBanner.show(uid, computerName, sender, text) }
	}

	Connections {
		target: App
		function onNotify(message, kind) { window.toast(message, kind) }
		function onGatewayAdded(name) {
			window.toast(qsTr("Menghubungkan ke %1…").arg(name.length > 0 ? name : qsTr("lokasi baru")), "info")
			Theme.prefs.onboardingDone = true
			if (!(stack.currentItem instanceof HomePage))
				window.goHome()
		}
	}
}
