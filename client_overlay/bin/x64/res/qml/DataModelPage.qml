import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3
import QtQuick.Dialogs 1.2
import pottedmeat7.walkinplace 1.0

MyStackGraphPage {
    id: dataModelPage
    name: "dataModelPage"

    property real timerInterval     : 1 / 4 * 1000

    property var hmdYPoints : []
    property var cntrl1YPoints : []
    property var cntrl2YPoints : []
    property var tracker1YPoints : []
    property var tracker2YPoints : []
    property var touchPoints : []   

    property var hmdSample : [0,0]
    property var cntrl1Sample : [0,0]
    property var cntrl2Sample : [0,0]
    property var trkr1Sample : [0,0]
    property var trkr2Sample : [0,0]

    property var showHMD : true
    property var showTRKR : false
    property var showCNTRL : false

    property real hmdPlotMaxPoint : 0.6
    property real hmdPlotMinPoint : -0.6
    property real cntrlPlotMaxPoint : 0.6
    property real cntrlPlotMinPoint : -0.6
    property real trkrPlotMaxPoint : 0.6
    property real trkrPlotMinPoint : -0.6

    property var modelGRPHOffset : 1

    property var initialLoaded: false

    function updateInfo() {  
    }

    property var startTimer: function() {
        getModelData()
        if ( WalkInPlaceTabController.isWIPEnabled() ) {
            refreshTimer.start()
        }
        updateInfo()
    }

    property var stopTimer: function() {
        modelGRPHOffset = 1

        hmdYPoints = []
        cntrl1YPoints = []
        cntrl2YPoints = []
        tracker1YPoints = []
        tracker2YPoints = []
        touchPoints = []   

        hmdSample = [0,0]
        cntrl1Sample = [0,0]
        cntrl2Sample = [0,0]
        trkr1Sample = [0,0]
        trkr2Sample = [0,0]

        hmdPlotMaxPoint = 0.6
        hmdPlotMinPoint = -0.6
        cntrlPlotMaxPoint = 0.6
        cntrlPlotMinPoint = -0.6
        trkrPlotMaxPoint = 0.6
        trkrPlotMinPoint = -0.6

        modelCanvas.requestPaint ();
        
    }

    content: Item {
        id: container

        GroupBox {
            Layout.fillWidth: true
            
            background: Rectangle {
                color: myPalette.base
                border.color: myPalette.base
                radius: 1
            }

            ColumnLayout {
                anchors.fill: parent
                width: 1700
                
                RowLayout {
                    width: 1700
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
                            stopTimer()
                            mainView.stopTimer()
                            var page = mainView.pop()
                        }
                    }

                    MyText {
                        id: headerTitle
                        text: "OpenVR-WalkInPlace"
                        Layout.maximumWidth: 640
                        Layout.minimumWidth: 640
                        Layout.preferredWidth: 640
                        font.pointSize: 22
                        anchors.verticalCenter: headerBackButton.verticalCenter
                        Layout.leftMargin: 30
                    }

                    MyText {
                        id: headerSpacer
                        text: " "
                        font.pointSize: 22
                        anchors.verticalCenter: headerBackButton.verticalCenter
                        Layout.maximumWidth: 120
                        Layout.minimumWidth: 120
                        Layout.preferredWidth: 120
                    }
                }

                GridLayout {
                    columns: 8

                    MyText {
                        text: " "
                        Layout.preferredWidth: 50
                    }  

                    MyToggleButton {
                        id: showHMDtoggle
                        text: "Show HMD"
                        checked: true
                        Layout.maximumWidth: 150
                        Layout.minimumWidth: 150
                        Layout.preferredWidth: 150
                        Layout.fillWidth: true
                        onCheckedChanged: {
                            if ( checked ) {
                                showTRKR = false
                                showCNTRL = false
                                showTrackers.checked = false
                                showControllers.checked = false
                            }
                            if ( showHMD != checked ) {
                                modelCanvas.requestPaint ();    
                                showHMD = checked 
                            }                       
                        }
                    }

                    MyText {
                        text: " "
                        Layout.preferredWidth: 70
                    }  

                    MyToggleButton {
                        id: showTrackers
                        text: "Show Trackers"
                        checked: false
                        Layout.maximumWidth: 200
                        Layout.minimumWidth: 200
                        Layout.preferredWidth: 200
                        Layout.fillWidth: true
                        onCheckedChanged: {
                            if ( checked ) {
                                showHMD = false
                                showCNTRL = false
                                showHMDtoggle.checked = false
                                showControllers.checked = false
                            }
                            if ( showTRKR != checked ) {
                                showTRKR = checked
                                modelCanvas.requestPaint ();
                            }
                        }
                    }

                    MyText {
                        text: " "
                        Layout.preferredWidth: 70
                    }  

                    MyToggleButton {
                        id: showControllers
                        text: "Show Controllers"
                        checked: false
                        Layout.maximumWidth: 300
                        Layout.minimumWidth: 300
                        Layout.preferredWidth: 300
                        Layout.fillWidth: true
                        onCheckedChanged: {
                            if ( checked ) {
                                showHMD = false
                                showCNTRL = false
                                showTrackers.checked = false
                                showHMDtoggle.checked = false
                            }
                            if ( showCNTRL != checked ) {
                                showCNTRL = checked
                                modelCanvas.requestPaint ();
                            }
                        }
                    }

                    MyText {
                        text: " "
                        Layout.preferredWidth: 70
                    }  

                    MyToggleButton {
                        id: showTouchLine
                        text: "Show Touch Line"
                        checked: false
                        Layout.maximumWidth: 200
                        Layout.minimumWidth: 200
                        Layout.preferredWidth: 200
                        Layout.fillWidth: true
                        onCheckedChanged: {
                            modelCanvas.requestPaint ();
                        }
                    }
                }
            }
        }

        Column {
            spacing: 6 
            anchors.fill: parent
            topPadding: 70

            GridLayout {
                columns: 1
                rows: 1
                width: 1800
                height: 900

                Canvas {
                    id: modelCanvas
                    width: 1800
                    height: 800
                    antialiasing: true
                    Layout.row: 1
                    Layout.column: 1
                    Layout.columnSpan: 1
                    Layout.rowSpan: 1

                    property int rectWidth: 1800
                    property int rectHeight: 800
                    property int circleRad: 15
                    property int topX : 50
                    property int topY : 0
                    property real vResolution: 0.25

                    onPaint: {
                        try {
                            var ctx = getContext("2d");                        
                            ctx.clearRect(0,0,modelCanvas.width, modelCanvas.height);
                            var yTicks = Math.ceil((hmdPlotMaxPoint-hmdPlotMinPoint)/vResolution);
                            var currentVal = hmdPlotMaxPoint
                            if ( showControllers.checked ) {
                                yTicks = Math.ceil((cntrlPlotMaxPoint-cntrlPlotMinPoint)/vResolution);
                                currentVal = cntrlPlotMaxPoint
                            } else if ( showTrackers.checked ) {
                                yTicks = Math.ceil((trkrPlotMaxPoint-trkrPlotMinPoint)/vResolution);
                                currentVal = trkrPlotMaxPoint
                            }
                            var graphXScale = ((rectWidth-topX) > hmdYPoints.length ? ((rectWidth-topX)/hmdYPoints.length) : (hmdYPoints.length/(rectWidth-topX)));
                            ctx.fillStyle = "#FFFFFF"
                            ctx.lineWidth = 1;
                            for(var y=topY; y<=rectHeight+topY; y+=Math.ceil(rectHeight/yTicks)){
                                ctx.fillText(currentVal.toFixed(2), 10, y);
                                ctx.beginPath();        
                                ctx.strokeStyle = "#666666";
                                ctx.moveTo(topX,y);
                                ctx.lineTo(rectWidth,y);
                                ctx.stroke();
                                ctx.closePath();
                                currentVal -= vResolution;
                            }
                            ctx.lineWidth = 2;
                            ctx.fillText(0, 10, rectHeight/2.0);
                            ctx.beginPath();        
                            ctx.strokeStyle = "#777777";
                            ctx.moveTo(topX,rectHeight/2.0);
                            ctx.lineTo(rectWidth,rectHeight/2.0);
                            ctx.stroke();
                            ctx.closePath();
                            if ( showHMDtoggle.checked ) {
                                var lastHMDYPoint = rectHeight/2 + topY
                                var lastX = 50;
                                var lastHMDSNYPoint = rectHeight/2 + topY
                                var validSNMki = hmdSample[1]
                                var r = (hmdPlotMaxPoint - hmdPlotMinPoint)  
                                var g = (modelCanvas.rectHeight)
                                for(var x=0; x<hmdYPoints.length; x++) {
                                    ctx.beginPath();  
                                    ctx.strokeStyle = "#DDDDDD";
                                    ctx.lineWidth = 1;
                                    ctx.moveTo(lastX,lastHMDYPoint);
                                    var val = hmdYPoints[x];
                                    val = (((val - hmdPlotMinPoint) * g) / r)
                                    ctx.lineTo(lastX+graphXScale,val+topY);
                                    lastHMDYPoint = val+topY;
                                    ctx.stroke();
                                    ctx.closePath();
                                    if ( x >= validSNMki && x-validSNMki < hmdSample.length-2) {
                                        ctx.beginPath();  
                                        ctx.strokeStyle = "#DD0000";
                                        if ( hmdSample[0] == 1 ) {
                                            ctx.strokeStyle = "#00DD00";
                                        }
                                        ctx.lineWidth = 2;
                                        ctx.moveTo(lastX,lastHMDSNYPoint);
                                        var val = hmdSample[x-validSNMki+2];
                                        val = (((val - hmdPlotMinPoint) * g) / r)
                                        ctx.lineTo(lastX+graphXScale,val+topY);
                                        lastHMDSNYPoint = val+topY;
                                        ctx.stroke();
                                        ctx.closePath();                          
                                    }
                                    lastX = lastX + graphXScale;
                                }
                            }
                            if ( showControllers.checked ) {
                                var lastcntrl1YPoint = rectHeight/2 + topY
                                var lastcntrl2YPoint = rectHeight/2 + topY
                                var lastX = 50
                                var lastcntrlSNYPoint = rectHeight/2 + topY
                                var validSNMki = cntrl1Sample[1]
                                var r = (cntrlPlotMaxPoint - cntrlPlotMinPoint)  
                                var g = (modelCanvas.rectHeight)
                                for(var x=0; x<cntrl1YPoints.length; x++) {
                                    ctx.beginPath();  
                                    ctx.strokeStyle = "#DDDDDD";
                                    ctx.lineWidth = 1;
                                    ctx.moveTo(lastX,lastcntrl1YPoint);
                                    var val = cntrl1YPoints[x];
                                    val = (((val - cntrlPlotMinPoint) * g) / r)
                                    ctx.lineTo(lastX+graphXScale,val+topY);
                                    lastcntrl1YPoint = val+topY;
                                    ctx.stroke();
                                    ctx.closePath();
                                    ctx.beginPath();  
                                    ctx.strokeStyle = "#999999";
                                    ctx.lineWidth = 1;
                                    ctx.moveTo(lastX,lastcntrl2YPoint);
                                    var val = cntrl2YPoints[x];
                                    val = (((val - cntrlPlotMinPoint) * g) / r)
                                    ctx.lineTo(lastX+graphXScale,val+topY);
                                    lastcntrl2YPoint = val+topY;
                                    ctx.stroke();
                                    ctx.closePath();
                                    if ( x >= validSNMki && x-validSNMki < cntrl1Sample.length-2) {
                                        ctx.beginPath();  
                                        ctx.strokeStyle = "#00DD00";
                                        ctx.lineWidth = 2;
                                        ctx.moveTo(lastX,lastcntrlSNYPoint);
                                        var val = cntrl1Sample[x-validSNMki+2];
                                        val = (((val - cntrlPlotMinPoint) * g) / r)
                                        ctx.lineTo(lastX+graphXScale,val+topY);
                                        lastcntrlSNYPoint = val+topY;
                                        ctx.stroke();
                                        ctx.closePath();                          
                                    }
                                    lastX = lastX + graphXScale;
                                }
                            }
                            if ( showTrackers.checked ) {
                                var lasttracker1YPoint = rectHeight/2 + topY
                                var lasttracker2YPoint = rectHeight/2 + topY
                                var lastX = 50;
                                var lasttrkrSNYPoint = rectHeight/2 + topY
                                var validSNMki = trkr1Sample[1]
                                var r = (trkrPlotMaxPoint - trkrPlotMinPoint)  
                                var g = (modelCanvas.rectHeight)
                                for(var x=0; x<tracker1YPoints.length; x++) {
                                    ctx.beginPath();  
                                    ctx.strokeStyle = "#DDDDDD";
                                    ctx.lineWidth = 1;
                                    ctx.moveTo(lastX,lasttracker1YPoint);
                                    var val = tracker1YPoints[x];
                                    val = (((val - trkrPlotMinPoint) * g) / r)
                                    ctx.lineTo(lastX+graphXScale,val+topY);
                                    lasttracker1YPoint = val+topY;
                                    ctx.stroke();
                                    ctx.closePath();
                                    ctx.beginPath();  
                                    ctx.strokeStyle = "#999999";
                                    ctx.lineWidth = 1;
                                    ctx.moveTo(lastX,lasttracker2YPoint);
                                    var val = tracker2YPoints[x];
                                    val = (((val - trkrPlotMinPoint) * g) / r)
                                    ctx.lineTo(lastX+graphXScale,val+topY);
                                    lasttracker2YPoint = val+topY;
                                    ctx.stroke();
                                    ctx.closePath();
                                    if ( x >= validSNMki && x-validSNMki < trkr1Sample.length-2) {
                                        ctx.beginPath();  
                                        ctx.strokeStyle = "#DD0000";
                                        if ( trkr1Sample[0] == 1 ) {
                                            ctx.strokeStyle = "#00DD00";
                                        }
                                        ctx.lineWidth = 2;
                                        ctx.moveTo(lastX,lasttrkrSNYPoint);
                                        var val = trkr1Sample[x-validSNMki+2];
                                        val = (((val - trkrPlotMinPoint) * g) / r)
                                        ctx.lineTo(lastX+graphXScale,val+topY);
                                        lasttrkrSNYPoint = val+topY;
                                        ctx.stroke();
                                        ctx.closePath();                          
                                    }
                                    lastX = lastX + graphXScale;
                                }
                            }
                            if ( showTouchLine.checked ) {
                                var lastTouchPoint = rectHeight/2 + topY;
                                var lastX = 50;
                                var gMax = hmdPlotMaxPoint
                                var gMin = hmdPlotMinPoint
                                if ( showControllers.checked ) {
                                    gMax = cntrlPlotMaxPoint
                                    gMin = cntrlPlotMinPoint
                                } else if ( showTrackers.checked ) {
                                    gMax = trkrPlotMaxPoint
                                    gMin = trkrPlotMinPoint
                                }
                                var r = (gMax - gMin)  
                                var g = (modelCanvas.rectHeight)
                                for(var x=0; x<touchPoints.length; x++) {
                                    ctx.beginPath();                
                                    ctx.strokeStyle = "#DDDD00";
                                    ctx.lineWidth = 1;
                                    ctx.moveTo(lastX,lastTouchPoint);
                                    var graphPoint = (-1*touchPoints[x]) * gMax;
                                    var finalPoint = (((graphPoint - gMin) * g) / r)
                                    var val = finalPoint;
                                    ctx.lineTo(lastX+graphXScale,val+topY);
                                    lastTouchPoint = val+topY;
                                    ctx.stroke();
                                    ctx.closePath();
                                    lastX = lastX + graphXScale;
                                }
                            }
                        } catch (error) {         
                            console.info(error.message)   
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

    function getModelData() {
        try {
            var velVals = WalkInPlaceTabController.getModelData()
            var i = 0
            while(i<velVals.length) {
                var velY = parseFloat(velVals[i]).toFixed(4);
                if ( Math.abs(velY) > hmdPlotMaxPoint ) {
                    hmdPlotMaxPoint = Math.abs(velY);
                    hmdPlotMinPoint = -hmdPlotMaxPoint;
                }
                velY = parseFloat(velVals[i+1]).toFixed(4);
                if ( Math.abs(velY) > cntrlPlotMaxPoint ) {
                    cntrlPlotMaxPoint = Math.abs(velY);
                    cntrlPlotMinPoint = -cntrlPlotMaxPoint;
                }
                velY = parseFloat(velVals[i+2]).toFixed(4);
                if ( Math.abs(velY) > cntrlPlotMaxPoint ) {
                    cntrlPlotMaxPoint = Math.abs(velY);
                    cntrlPlotMinPoint = -cntrlPlotMaxPoint;
                }
                velY = parseFloat(velVals[i+3]).toFixed(4);
                if ( Math.abs(velY) > trkrPlotMaxPoint ) {
                    trkrPlotMaxPoint = Math.abs(velY);
                    trkrPlotMinPoint = -trkrPlotMaxPoint;
                }
                velY = parseFloat(velVals[i+4]).toFixed(4);
                if ( Math.abs(velY) > trkrPlotMaxPoint ) {
                    trkrPlotMaxPoint = Math.abs(velY);
                    trkrPlotMinPoint = -trkrPlotMaxPoint;
                }
                i = i+6
            }
            i = 0
            while(i<velVals.length) {
                var velY = parseFloat(velVals[i]).toFixed(4);
                hmdYPoints.push(velY);

                velY = parseFloat(velVals[i+1]).toFixed(4);
                cntrl1YPoints.push(velY);

                velY = parseFloat(velVals[i+2]).toFixed(4);
                cntrl2YPoints.push(velY);

                velY = parseFloat(velVals[i+3]).toFixed(4);
                tracker1YPoints.push(velY);

                velY = parseFloat(velVals[i+4]).toFixed(4);
                tracker2YPoints.push(velY);

                touchPoints.push(velVals[i+5])

                i = i + 6;
            }

            modelCanvas.requestPaint ();

        } catch (error) {         
            console.info(error.message)   
        }
    }

    Timer {
        id: refreshTimer
        interval: timerInterval
        running: false
        repeat: true
        onTriggered: {
            try {
                if ( showHMDtoggle.checked ) {
                    var temp = WalkInPlaceTabController.getHMDSample()
                    hmdSample = []
                    for(var i=0; i<temp.length; i+=1 ) {
                        if ( i == 0 ) {
                            hmdSample.push(temp[i].toFixed(0))
                        } else if ( i == 1 ) {
                            hmdSample.push((temp[i]).toFixed(0))
                        } else {
                            hmdSample.push(temp[i].toFixed(4))
                        }
                    }
                }
                else if ( showControllers.checked ) {
                    var temp = WalkInPlaceTabController.getCNTRLSample()
                    cntrl1Sample = []
                    cntrl2Sample = []
                    for(var i=0; i<temp.length-1; i+=2 ) {
                        if ( i < 2 ) {
                            cntrl1Sample.push(temp[0].toFixed(0))
                            cntrl2Sample.push(temp[0].toFixed(0))
                            cntrl1Sample.push((temp[1]).toFixed(0))
                            cntrl2Sample.push((temp[1]).toFixed(0))
                        } else {
                            cntrl1Sample.push(temp[i].toFixed(4))
                            cntrl2Sample.push(temp[i+1].toFixed(4))
                        }
                    }
                }
                else if ( showTrackers.checked ) {
                    var temp = WalkInPlaceTabController.getTRKRSample()
                    trkr1Sample = []
                    trkr2Sample = []
                    for(var i=0; i<temp.length; i+=2 ) {
                        if ( i < 2 ) {
                            trkr1Sample.push(temp[0].toFixed(0))
                            trkr2Sample.push(temp[0].toFixed(0))
                            trkr1Sample.push((temp[1]).toFixed(0))
                            trkr2Sample.push((temp[1]).toFixed(0))
                        } else {
                            var r = (trkrPlotMaxPoint - trkrPlotMinPoint)  
                            var g = (modelCanvas.rectHeight)
                            trkr1Sample.push(temp[i].toFixed(4))
                            trkr2Sample.push(temp[i+1].toFixed(4))
                        }
                    }
                }
                modelCanvas.requestPaint ();
            } catch (error) {         
                console.info(error.message)   
            }
        }
    }

}