import QtQuick

// themed text; "style" picks a type scale entry
Text {
	property string style: "body"   // display, title, heading, body, label, caption
	property bool muted: false

	color: muted ? Theme.textMuted : Theme.text
	font.family: Theme.fontFamily
	font.pixelSize: {
		switch (style) {
		case "display": return Theme.fontDisplay
		case "title": return Theme.fontTitle
		case "heading": return Theme.fontHeading
		case "label": return Theme.fontLabel
		case "caption": return Theme.fontCaption
		default: return Theme.fontBody
		}
	}
	font.weight: {
		switch (style) {
		case "display": return Font.ExtraBold
		case "title": return Font.Bold
		case "heading": return Font.Bold
		case "label": return Font.DemiBold
		default: return Font.Normal
		}
	}
	lineHeight: style === "body" || style === "caption" ? 1.25 : 1.1
	wrapMode: Text.Wrap
	textFormat: Text.PlainText
}
