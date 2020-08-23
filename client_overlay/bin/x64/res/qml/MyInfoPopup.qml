import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3

Popup {
    id: myDialogPopup

    implicitHeight: parent.height
    implicitWidth: parent.width

    property string dialogTitle: ""
    property string dialogText1: ""
    property string dialogText2: ""
    property string dialogText3: ""
    property string dialogText4: ""
    property string dialogText5: ""
    property int dialogWidth: 1150
    property int dialogHeight: 650

    property Item dialogContentItem1: MyText {
        text: dialogText1
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        Layout.fillWidth: true
    }
    
    property Item dialogContentItem2: MyText {
        text: dialogText2
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        Layout.fillWidth: true
    }

    property Item dialogContentItem3: MyText {
        text: dialogText3
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        Layout.fillWidth: true
    }

    property Item dialogContentItem4: MyText {
        text: dialogText4
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        Layout.fillWidth: true
    }

    property Item dialogContentItem5: MyText {
        text: dialogText5
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        Layout.fillWidth: true
    }

    property bool okClicked: false

    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

    background: Rectangle {
        color: "black"
        opacity: 0.8
     }

    contentItem: Item {
        Rectangle {
            implicitWidth: dialogWidth
            implicitHeight: dialogHeight
            anchors.centerIn: parent
            radius: 24
            color: myPalette.mid // "#c0c0c0" // #0E5B36"
            border.color: "#000000"
            border.width: 2
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                MyText {
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    text: dialogTitle
                }
                Rectangle {
                    color: "#dddddd"
                    height: 1
                    Layout.fillWidth: true
                }
                Item {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                }
                ColumnLayout {
                    id: dialogContent
                }
                Item {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 24
                    Layout.rightMargin: 24
                    Layout.bottomMargin: 12
                    MyPushButton {
                        implicitWidth: 200
                        text: "Ok"
                        onClicked: {
                            okClicked = true
                            myDialogPopup.close()
                        }
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    MyPushButton {
                        implicitWidth: 200
                        text: "Cancel"
                        onClicked: {
                            okClicked = false
                            myDialogPopup.close()
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        dialogContentItem1.parent = dialogContent
        dialogContentItem2.parent = dialogContent
        dialogContentItem3.parent = dialogContent
        dialogContentItem4.parent = dialogContent
    }
}
