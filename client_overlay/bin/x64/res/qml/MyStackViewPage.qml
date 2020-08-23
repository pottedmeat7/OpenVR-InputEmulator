import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.2
import "." 

Rectangle {
    width: 1500
    height: 800
    color: myPalette.base

    property string name : "default"

    property StackView stackView

    property Item content: Frame {
        background: Rectangle {
            color: myPalette.base
        }
        MyText {
            text: "Content"
        }
    }

    ColumnLayout {
        id: mainLayout
        spacing: 7
        anchors.fill: parent
    }

    Component.onCompleted: {
        content.parent = mainLayout
        content.Layout.fillHeight = true
        content.Layout.fillWidth = true
        content.Layout.topMargin = 0
        content.Layout.leftMargin = 0
        content.Layout.rightMargin = 0
        content.Layout.bottomMargin = 0
    }
}
