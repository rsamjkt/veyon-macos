pragma Singleton
import QtQuick
import QtCore

// Design tokens for AruniControl Mobile - warm "sunrise" palette matching the
// desktop apps, with light and dark variants
QtObject {
	id: theme

	// 0 = follow system, 1 = light, 2 = dark
	property int mode: prefs.themeMode
	readonly property bool dark: mode === 2 || (mode === 0 && Qt.styleHints.colorScheme === Qt.ColorScheme.Dark)

	property Settings prefs: Settings {
		category: "ui"
		property int themeMode: 0
		property int gridSize: 1   // 0 = small, 1 = medium, 2 = large
		property bool remoteHintShown: false
		property bool onboardingDone: false
	}

	function setMode(m) { prefs.themeMode = m }

	// brand
	readonly property color accent: "#F2812F"
	readonly property color accentPressed: "#D9661A"
	readonly property color accentSoft: dark ? "#3B2616" : "#FFEEDF"
	readonly property color textOnAccent: "#FFFFFF"
	readonly property color gradientTop: "#FFB054"
	readonly property color gradientBottom: "#E0602A"

	// surfaces
	readonly property color background: dark ? "#14110F" : "#F7F4F0"
	readonly property color surface: dark ? "#1E1A17" : "#FFFFFF"
	readonly property color surfaceAlt: dark ? "#29241F" : "#F1ECE6"
	readonly property color surfaceHigh: dark ? "#332D27" : "#E9E3DC"
	readonly property color border: dark ? "#3A332D" : "#E6DFD7"
	readonly property color scrim: dark ? "#B3000000" : "#66000000"
	readonly property color screenBackground: "#0E0C0B"

	// text
	readonly property color text: dark ? "#F4EFE9" : "#1E1B18"
	readonly property color textMuted: dark ? "#AAA299" : "#6D665F"
	readonly property color textFaint: dark ? "#7A726A" : "#9C958D"

	// status
	readonly property color success: dark ? "#3DBE72" : "#1F9D55"
	readonly property color warning: dark ? "#F2B533" : "#D48E00"
	readonly property color danger: dark ? "#FF6B6B" : "#DC3B3F"
	readonly property color info: dark ? "#6AA8FF" : "#2F6FE4"
	readonly property color successSoft: dark ? "#16301F" : "#E3F5EA"
	readonly property color dangerSoft: dark ? "#3A1C1C" : "#FDE7E7"
	readonly property color infoSoft: dark ? "#172841" : "#E6EFFD"

	function statusColor(status) {
		switch (status) {
		case "online": return success
		case "connecting": return warning
		case "denied": return danger
		case "noservice": return warning
		default: return textFaint
		}
	}

	// shape
	readonly property int radiusSmall: 10
	readonly property int radius: 16
	readonly property int radiusLarge: 24
	readonly property int radiusSheet: 28

	// spacing
	readonly property int gap: 8
	readonly property int pad: 16
	readonly property int padLarge: 24

	// type
	property FontLoader brandFont: FontLoader { source: "fonts/PlusJakartaSans.ttf" }
	readonly property string fontFamily: brandFont.status === FontLoader.Ready ? brandFont.name : Qt.application.font.family
	readonly property int fontDisplay: 30
	readonly property int fontTitle: 22
	readonly property int fontHeading: 18
	readonly property int fontBody: 15
	readonly property int fontLabel: 14
	readonly property int fontCaption: 12

	// motion
	readonly property int fast: 120
	readonly property int normal: 220
	readonly property int slow: 360
}
