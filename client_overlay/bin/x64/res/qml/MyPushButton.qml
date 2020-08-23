import QtQuick 2.7
import QtQuick.Controls 2.0
import "." 

Button {
	hoverEnabled: true
	contentItem: MyText {
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        text: parent.text
        font.pointSize: 19
        color: parent.enabled ? myPalette.buttonText : "#737373"
	}
	background: Rectangle {
        border.color: parent.down ? myPalette.highlight : (parent.activeFocus ? myPalette.highlight : myPalette.base)
        color: myPalette.button //parent.down ? "#c0c0c0" : (parent.activeFocus ? "#c0c0c0" : "#c0c0c0")
    }

    onHoveredChanged: {
        if (hovered) {
            forceActiveFocus()
        } else {
            focus = false
        }
    }

    onClicked: {
    }
}
