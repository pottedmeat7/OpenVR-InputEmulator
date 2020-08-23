import QtQuick 2.7
import QtQuick.Controls 2.0
import "." 


ComboBox {
    id: myComboBox
    hoverEnabled: true

    background: Rectangle {
        border.color: parent.down ? myPalette.base : (parent.activeFocus ? myPalette.base : myPalette.base)
        color: parent.down ? myPalette.base : (parent.activeFocus ? myPalette.base : myPalette.mid )
        height: 36
    }

    contentItem: MyText {
        leftPadding: 7
        rightPadding: parent.indicator.width + parent.spacing
        text: parent.displayText
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    delegate: ItemDelegate {
        width: myComboBox.width
        text: modelData
        hoverEnabled: true
        contentItem: MyText {
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            text: parent.text
            color: myPalette.windowText //parent.enabled ? "#000000" : "#000000"
        }
        background: Rectangle {
            color: parent.pressed ? myPalette.base : (parent.hovered ? myPalette.base : myPalette.mid)
        }
    }

    indicator: Canvas {
        x: parent.width - width - parent.rightPadding
        y: parent.topPadding + (parent.availableHeight - height) / 2
        width: 13
        height: 7
        contextType: "2d"

        onPaint: {
            if (!context) {
                getContext("2d")
            }
            context.reset();
            context.lineWidth = 2
            context.moveTo(1, 1);
            context.lineTo(Math.ceil(width / 2), height - 1);
            context.lineTo(width - 1, 1);
            context.strokeStyle = myPalette.windowText //"#000000"
            context.stroke()
        }
    }

    onHoveredChanged: {
        if (hovered) {
            forceActiveFocus()
        }
    }

    onActivated: {
    }

    Component.onCompleted: {
        popup.background.color = myPalette.mid //"#c0c0c0"
    }
}
