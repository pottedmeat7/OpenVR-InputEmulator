import QtQuick 2.7
import QtQuick.Controls 2.0
import "." 


CheckBox {
	hoverEnabled: true
	spacing: 12

	indicator: Rectangle {
		implicitWidth: 28
		implicitHeight: 28
		x: parent.leftPadding
		y: parent.height / 2 - height / 2
		color: parent.activeFocus ? myPalette.mid : myPalette.base
		border.width: 0
		Image {
			width: 38
			height: 38
			x: (parent.width - width) / 2
			y: (parent.height - height) / 2
			source: "image://default/check/#ffffff"
			sourceSize.width: width
			sourceSize.height: height
			visible: parent.parent.checked
		}
	}

	contentItem: MyText {
        text: parent.text
        horizontalAlignment: Text.AlignLeft
		verticalAlignment: Text.AlignVCenter
        leftPadding: parent.indicator.width + parent.spacing
        color: myPalette.windowText //parent.enabled ? "#000000" : "#000000"
	}

	background: Rectangle {
		color: myPalette.base // "#c3c3c3"
        opacity: parent.activeFocus ? 1.0 : 0.0
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
