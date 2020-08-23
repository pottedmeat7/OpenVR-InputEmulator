import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3
import QtQuick.Dialogs 1.2
import pottedmeat7.walkinplace 1.0

MyStackViewPage {
    id: paceControlPage
    name: "paceControlPage"
    property var initialLoaded: false

    property var minTouch : 0.2
    property var midTouch : 0.5
    property var maxTouch : 1.0

    property var thresholdStep : 0.01

    property var startTimer: function() {
        updateInfo()
    }

    function updateInfo() {      
        setMinTouch(WalkInPlaceTabController.getMinTouch())
        setMidTouch(WalkInPlaceTabController.getMidTouch())
        setMaxTouch(WalkInPlaceTabController.getMaxTouch())
        paceControlDialog.currentIndex = WalkInPlaceTabController.getPaceControl()
    }

    content: ColumnLayout {
        anchors.top: parent.top
        Layout.leftMargin: 100
        spacing: 7
        Layout.alignment: Qt.AlignHCenter

        GroupBox {
            Layout.fillWidth: true
            
            background: Rectangle {
                color: myPalette.base
                border.color: myPalette.base
                radius: 1
            }

            ColumnLayout {
                anchors.fill: parent
                Layout.alignment: Qt.AlignHCenter

                RowLayout {
                    Button {
                        id: headerBackButton
                        Layout.preferredHeight: 60
                        Layout.preferredWidth: 60
                        hoverEnabled: true
                        enabled: true
                        opacity: 1.0
                        contentItem: Image {
                            source: "backarrow.svg"
                            sourceSize.width: 60
                            sourceSize.height: 60
                            anchors.fill: parent
                        }
                        background: Rectangle {
                            opacity: parent.down ? 1.0 : (parent.activeFocus ? 0.5 : 0.0)
                            color: myPalette.base
                            radius: 4
                            anchors.fill: parent
                        }
                        onHoveredChanged: {
                            if (hovered) {
                                forceActiveFocus()
                            } else {
                                focus = false
                            }
                        }
                        onClicked: {
                            mainView.stopTimer()
                            var page = mainView.pop()
                        }
                    }

                    MyText {
                        id: headerTitle
                        text: "OpenVR-WalkInPlace"
                        font.pointSize: 24
                    }
                }
            }
        }

        ColumnLayout {
            spacing: 7
            Layout.alignment: Qt.AlignHCenter

            GroupBox {
                Layout.minimumWidth: 1200
                Layout.alignment: Qt.AlignHCenter
                anchors.centerIn: parent

                label: MyText {
                    text: "Input Scale Config"
                }

                background: Rectangle {
                    color: myPalette.mid
                    border.color: myPalette.mid
                    radius: 1
                }

                ColumnLayout {
                    anchors.fill: parent

                    GridLayout {
                        columns: 6

                        MyText {
                            text: "Slow Pace Touch"
                            horizontalAlignment: Text.AlignHCenter
                            Layout.preferredWidth: 220
                        }

                        MyText {
                            text: " "
                            Layout.preferredWidth: 70
                        }  

                        MyText {
                            text: "Medium Pace Touch"
                            horizontalAlignment: Text.AlignHCenter
                            Layout.preferredWidth: 220
                        }

                        MyText {
                            text: " "
                            Layout.preferredWidth: 70
                        }  

                        MyText {
                            text: "Max Pace Touch"
                            horizontalAlignment: Text.AlignHCenter
                            Layout.preferredWidth: 220
                        }

                        MyText {
                            text: " "
                            Layout.preferredWidth: 70
                        }  
                    }

                    GridLayout {
                        columns: 15

                        MyPushButton {
                            id: minTouchMinusButton
                            Layout.preferredWidth: 40
                            text: "-"
                            onClicked: {
                                var val = minTouch - thresholdStep
                                setMinTouch(val)
                            }
                        }

                        MyTextField {
                            id: minTouchInputField
                            text: "0.2"
                            keyBoardUID: 120
                            Layout.preferredWidth: 186
                            horizontalAlignment: Text.AlignHCenter
                            function onInputEvent(input) {
                                var val = parseFloat(input)
                                if (!isNaN(val)) {
                                    setMinTouch(val)
                                } 
                            }
                        }

                        MyPushButton {
                            id: minTouchPlusButton
                            Layout.preferredWidth: 40
                            text: "+"
                            onClicked: {
                                var val = minTouch + thresholdStep
                                setMinTouch(val)
                            }
                        }

                        MyText {
                            text: " "
                            Layout.preferredWidth: 70
                        }  

                        MyPushButton {
                            id: midTouchMinusButton
                            Layout.preferredWidth: 40
                            text: "-"
                            onClicked: {
                                var val = midTouch - thresholdStep
                                setMidTouch(val)
                            }
                        }

                        MyTextField {
                            id: midTouchInputField
                            text: "0.5"
                            keyBoardUID: 121
                            Layout.preferredWidth: 186
                            horizontalAlignment: Text.AlignHCenter
                            function onInputEvent(input) {
                                var val = parseFloat(input)
                                if (!isNaN(val)) {
                                    setMidTouch(val)
                                } 
                            }
                        }

                        MyPushButton {
                            id: midTouchPlusButton
                            Layout.preferredWidth: 40
                            text: "+"
                            onClicked: {
                                var val = midTouch + thresholdStep
                                setMidTouch(val)
                            }
                        }

                        MyText {
                            text: " "
                            Layout.preferredWidth: 70
                        }  

                        MyPushButton {
                            id: maxTouchMinusButton
                            Layout.preferredWidth: 40
                            text: "-"
                            onClicked: {
                                var val = maxTouch - thresholdStep
                                setMaxTouch(val)
                            }
                        }

                        MyTextField {
                            id: maxTouchInputField
                            text: "1.0"
                            keyBoardUID: 122
                            Layout.preferredWidth: 186
                            horizontalAlignment: Text.AlignHCenter
                            function onInputEvent(input) {
                                var val = parseFloat(input)
                                if (!isNaN(val)) {
                                    setMaxTouch(val)
                                } 
                            }
                        }

                        MyPushButton {
                            id: maxTouchPlusButton
                            Layout.preferredWidth: 40
                            text: "+"
                            onClicked: {
                                var val = maxTouch + thresholdStep
                                setMaxTouch(val)
                            }
                        }

                        MyText {
                            text: " "
                            Layout.preferredWidth: 70
                        }  
                    }
                }
            }
        }

        ColumnLayout {
            spacing: 7
            Layout.alignment: Qt.AlignHCenter

            GroupBox {
                Layout.minimumWidth: 1200
                Layout.alignment: Qt.AlignHCenter
                anchors.centerIn: parent

                label: MyText {
                    text: "Determine input scale with"
                }

                background: Rectangle {
                    color: myPalette.mid
                    border.color: myPalette.mid
                    radius: 1
                }

                ColumnLayout {
                    anchors.fill: parent

                    GridLayout {
                    columns: 1

                        MyComboBox {
                            id: paceControlDialog 
                            currentIndex: 0
                            Layout.maximumWidth: 800
                            Layout.minimumWidth: 800
                            Layout.preferredWidth: 800
                            Layout.preferredHeight: 40
                            Layout.fillWidth: true
                            displayText: currentText
                            model: ["Average Controller Model result", "Average HMD Model result", "Average Controller and HMD Pace", "Arm Swing Controller Pace"]
                            onCurrentIndexChanged: {
                                if (currentIndex >= 0) { 
                                    WalkInPlaceTabController.setPaceControl(currentIndex)       
                                } 
                            }
                        }
                    }
                }
            }
        }
        
        Component.onCompleted: {   
            if ( !initialLoaded ) { 
                updateInfo()
            }
            initialLoaded = true
        }
    }

    function stopTimer() {

    }

    function setMinTouch(val) {
        minTouch = val
        WalkInPlaceTabController.setMinTouch(val)
        minTouchInputField.text = val.toFixed(2).toString()
    }

    function setMidTouch(val) {
        midTouch = val
        WalkInPlaceTabController.setMidTouch(val)
        midTouchInputField.text = val.toFixed(2).toString()
    }

    function setMaxTouch(val) {
        maxTouch = val
        WalkInPlaceTabController.setMaxTouch(val)
        maxTouchInputField.text = val.toFixed(2).toString()
    }
}