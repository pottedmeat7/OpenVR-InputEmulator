import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3
import QtQuick.Dialogs 1.2
import pottedmeat7.walkinplace 1.0

MyStackViewPage {
    id: graphPage
    name: "graphPage"

    property real timerInterval     : 1 / 40 * 1000

    property var hmdYROTVELPoints : []
    property var hmdYVELPoints : []
    property var hmdPROTVELPoints : []

    property var cntrlYPoints : []
    property var cntrl2YPoints : []

    property var trkr1YPoints : []
    property var trkr2YPoints : []

    property real hmdPlotMaxPoint : 0.6
    property real hmdPlotMinPoint : -0.6

    property real cntrlPlotMaxPoint : 1.0
    property real cntrlPlotMinPoint : -1.0

    property real trkrPlotMaxPoint : 0.6
    property real trkrPlotMinPoint : -0.6

    property int autoConfMode          : -2

    property real totalSampleTime      : 25000

    property real currentSampleTime    : 0

    property int sampleState : 0

    property int sampleTime : 0
    property int sampleLimit : 5000

    property real tdiff : 0

    property var startTimer: function() {
        refreshTimer.start()
    }
    
    property var stopTimer: function() {
        refreshTimer.stop()
        hmdYROTVELPoints = []
        hmdYVELPoints = []
        hmdPROTVELPoints = []

        cntrlYPoints = []
        cntrl2YPoints = []

        trkr1YPoints = []
        trkr2YPoints = []

        hmdPlotMaxPoint = 0.6
        hmdPlotMinPoint = -0.6

        cntrlPlotMaxPoint = 0.6
        cntrlPlotMinPoint = -0.6

        trkrPlotMaxPoint = 0.6
        trkrPlotMinPoint = -0.6
    }

    property var resetGraph: function() {
        var r = (hmdPlotMaxPoint - hmdPlotMinPoint)  
        var g = (hmdCanvas.rectHeight)
        var velRes = (((0 - hmdPlotMinPoint) * g) / r)
        while ( hmdYROTVELPoints.length < hmdCanvas.seconds*hmdCanvas.sections ) {
            hmdYROTVELPoints.push(velRes);
            hmdYVELPoints.push(velRes);
            hmdPROTVELPoints.push(velRes);
        }
        var r = (cntrlPlotMaxPoint - cntrlPlotMinPoint)  
        var g = (cntrlCanvas.rectHeight)
        var velRes = (((0 - cntrlPlotMinPoint) * g) / r)
        while ( cntrlYPoints.length < cntrlCanvas.seconds*cntrlCanvas.sections ) {
            cntrlYPoints.push(velRes);
        }
        while ( cntrl2YPoints.length < cntrlCanvas.seconds*cntrlCanvas.sections ) {
            cntrl2YPoints.push(velRes);
        }
        var r = (trkrPlotMaxPoint - trkrPlotMinPoint)  
        var g = (trkrCanvas.rectHeight)
        var velRes = (((0 - trkrPlotMinPoint) * g) / r)
        while ( trkr1YPoints.length < trkrCanvas.seconds*trkrCanvas.sections ) {
            trkr1YPoints.push(velRes);
        }
        while ( trkr2YPoints.length < trkrCanvas.seconds*trkrCanvas.sections ) {
            trkr2YPoints.push(velRes);
        }
    }
    

    content: Item {
        id:container

        GroupBox {
            Layout.fillWidth: true
            
            background: Rectangle {
                color: myPalette.base
                border.color: myPalette.base
                radius: 1
            }

            ColumnLayout {
                anchors.fill: parent
                width: 1500
                
                RowLayout {
                    width: 1500
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
                            stopTimer()
                            resetGraph()
                            autoConfMode = -2
                            sampleTime = 0
                            autoConfigPopup.setTitle("Walking Pace Config")
                            autoConfigPopup.setTextDetail("Start Walking IN PLACE in")
                            autoConfigPopup.setTimeout(5)
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
            }
        }

        Column {
            spacing: 6 
            anchors.fill: parent
            topPadding: 80

            GridLayout {
                columns: 2
                rows: 2
                width: 1500
                height: 700

                Canvas {
                    id: hmdCanvas
                    width: 750
                    height: 300
                    antialiasing: true
                    Layout.row: 1
                    Layout.column: 1
                    Layout.columnSpan: 1
                    Layout.rowSpan: 1

                    property int rectWidth: 750
                    property int rectHeight: 300
                    property int circleRad: 15
                    property int topX : 50
                    property int topY : 0
                    property real resolution: 0.1
                    property int seconds : 5
                    property int sections : 5
                    property int period : (rectWidth/(seconds*sections)) 

                    onPaint: {
                        try {
                            var ctx = getContext("2d");                        
                            ctx.clearRect(0,0,hmdCanvas.width, hmdCanvas.height);
                            ctx.font = "16px sans-serif";
                            ctx.fillStyle = "#222222"
                            ctx.fillRect(0, topY, rectWidth, rectHeight);  
                            ctx.fillStyle = "#FFFFFF"
                            ctx.fillText("HMD", 90, 35);
                            ctx.fillText("Y_VEL", 239, 35);
                            ctx.fillText("YAW", 342, 35);
                            ctx.fillText("PITCH", 442, 35); 
                            ctx.fillStyle = "#00DD00"
                            ctx.fillRect(293,17,20,20);   
                            ctx.fillStyle = "#DD0000"
                            ctx.fillRect(384,17,20,20);   
                            ctx.fillStyle = "#0000DD"
                            ctx.fillRect(497,17,20,20);   
                            var yTicks = Math.ceil((hmdPlotMaxPoint-hmdPlotMinPoint)/resolution);
                            var currentVal = hmdPlotMaxPoint
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
                                currentVal -= resolution;
                            }
                            ctx.lineWidth = 2;
                            ctx.fillText(0, 10, (rectHeight/2.0).toFixed(1));
                            ctx.beginPath();        
                            ctx.strokeStyle = "#777777";
                            ctx.moveTo(topX,rectHeight/2.0);
                            ctx.lineTo(rectWidth,rectHeight/2.0);
                            ctx.stroke();
                            ctx.closePath();
                            var lastHMDYROTPoint = rectHeight/2 + topY
                            var lastHMDYPoint = rectHeight/2 + topY
                            var lastHMDPROTPoint = rectHeight/2 + topY
                            var lastX = topX;
                            for(var x=topX; x<rectWidth; x+=period) {
                                if ( hmdYROTVELPoints.length-1 > (x-topX)/period) {
                                    ctx.beginPath();                
                                    ctx.strokeStyle = "#DD0000";
                                    ctx.lineWidth = 1;
                                    ctx.moveTo(lastX,lastHMDYROTPoint);
                                    var val = hmdYROTVELPoints[Math.floor((x-topX)/period)];
                                    ctx.lineTo(x,val+topY);
                                    lastHMDYROTPoint = val+topY;
                                    ctx.stroke();
                                    ctx.closePath();
                                }  
                                if ( hmdYVELPoints.length-1 > (x-topX)/period) {
                                    ctx.beginPath();  
                                    ctx.strokeStyle = "#00DD00";
                                    ctx.lineWidth = 1;
                                    ctx.moveTo(lastX,lastHMDYPoint);
                                    var val = hmdYVELPoints[Math.floor((x-topX)/period)];
                                    ctx.lineTo(x,val+topY);
                                    lastHMDYPoint = val+topY;
                                    ctx.stroke();
                                    ctx.closePath();
                                }  
                                if ( hmdPROTVELPoints.length-1 > (x-topX)/period) {
                                    ctx.beginPath();  
                                    ctx.strokeStyle = "#0000DD";
                                    ctx.lineWidth = 1;
                                    ctx.moveTo(lastX,lastHMDPROTPoint);
                                    var val = hmdPROTVELPoints[Math.floor((x-topX)/period)];
                                    ctx.lineTo(x,val+topY);
                                    lastHMDPROTPoint = val+topY;
                                    ctx.stroke();
                                    ctx.closePath();
                                }  
                                lastX = x;
                            }
                        } catch (error) {         
                            console.info(error.message)   
                        }
                    }
                }


                Canvas {
                    id: cntrlCanvas
                    width: 750
                    height: 700
                    antialiasing: true
                    Layout.row: 1
                    Layout.column: 2
                    Layout.columnSpan: 1
                    Layout.rowSpan: 2

                    property int rectWidth: 750
                    property int rectHeight: 700
                    property int circleRad: 15
                    property int topX : 25
                    property int topY : 0
                    property real resolution: 0.25
                    property int seconds : 5
                    property int sections : 5
                    property int period : (rectWidth/(seconds*sections)) // width at 40 mHz for 5 seconds

                    onPaint: {
                        try {
                            var ctx = getContext("2d");                        
                            ctx.clearRect(0,0,cntrlCanvas.width, cntrlCanvas.height);
                            var yTicks = Math.ceil((cntrlPlotMaxPoint-cntrlPlotMinPoint)/resolution);
                            ctx.fillStyle = "#FFFFFF"
                            ctx.lineWidth = 1;
                            var currentVal = cntrlPlotMaxPoint
                            for(var y=topY; y<=rectHeight+topY; y+=Math.ceil(rectHeight/yTicks)){
                                ctx.fillText(currentVal.toFixed(2), 10, y);
                                ctx.beginPath();        
                                ctx.strokeStyle = "#666666";
                                ctx.moveTo(topX,y);
                                ctx.lineTo(rectWidth,y);
                                ctx.stroke();
                                ctx.closePath();
                                currentVal -= resolution;
                            }
                            ctx.lineWidth = 2;
                            ctx.fillText(0, 10, (rectHeight/2.0).toFixed(1));
                            ctx.beginPath();        
                            ctx.strokeStyle = "#777777";
                            ctx.moveTo(topX,rectHeight/2.0);
                            ctx.lineTo(rectWidth,rectHeight/2.0);
                            ctx.stroke();
                            ctx.closePath();
                            var lastcntrlYPoint = rectHeight/2 + topY
                            var lastcntrl2YPoint = rectHeight/2 + topY
                            var lastX = topX;
                            for(var x=topX; x<rectWidth; x+=period) {
                                if ( cntrlYPoints.length-1 > (x-topX)/period) {
                                    ctx.beginPath();  
                                    ctx.strokeStyle = "#00DD00";
                                    ctx.lineWidth = 1;
                                    ctx.moveTo(lastX,lastcntrlYPoint);
                                    var val = cntrlYPoints[Math.floor((x-topX)/period)];
                                    ctx.lineTo(x,val+topY);
                                    lastcntrlYPoint = val+topY;
                                    ctx.stroke();
                                    ctx.closePath();
                                }  
                                if ( cntrl2YPoints.length-1 > (x-topX)/period) {
                                    ctx.beginPath();  
                                    ctx.strokeStyle = "#00DDDD";
                                    ctx.lineWidth = 1;
                                    ctx.moveTo(lastX,lastcntrl2YPoint);
                                    var val = cntrl2YPoints[Math.floor((x-topX)/period)];
                                    ctx.lineTo(x,val+topY);
                                    lastcntrl2YPoint = val+topY;
                                    ctx.stroke();
                                    ctx.closePath();
                                }  
                                lastX = x;
                            }
                        } catch (error) {         
                            console.info(error.message)   
                        }
                    }
                }  

                Canvas {
                    id: trkrCanvas
                    width: 750
                    height: 400
                    antialiasing: true     
                    Layout.row: 2
                    Layout.column: 1
                    Layout.columnSpan: 1
                    Layout.rowSpan: 1

                    property int rectWidth: 750
                    property int rectHeight: 400
                    property int circleRad: 15
                    property int topX : 50
                    property int topY : 0
                    property real resolution: 0.1
                    property int seconds : 5
                    property int sections : 5
                    property int period : (rectWidth/(seconds*sections)) // width at 10 mHz for 10 seconds

                    onPaint: {
                        try {
                            var ctx = getContext("2d");                        
                            ctx.clearRect(0,0,trkrCanvas.width, trkrCanvas.height);
                            var yTicks = Math.ceil((trkrPlotMaxPoint-trkrPlotMinPoint)/resolution);
                            var currentVal = trkrPlotMaxPoint
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
                                currentVal -= resolution;
                            }
                            ctx.lineWidth = 2;
                            ctx.fillText(0, 10, (rectHeight/2.0).toFixed(1));
                            ctx.beginPath();        
                            ctx.strokeStyle = "#777777";
                            ctx.moveTo(topX,rectHeight/2.0);
                            ctx.lineTo(rectWidth,rectHeight/2.0);
                            ctx.stroke();
                            ctx.closePath();
                            var lasttrkr1YPoint = rectHeight/2 + topY
                            var lasttrkr2YPoint = rectHeight/2 + topY
                            var lastX = topX;
                            for(var x=topX; x<rectWidth; x+=period) {
                                if ( trkr1YPoints.length-1 > (x-topX)/period) {
                                    ctx.beginPath();  
                                    ctx.strokeStyle = "#00DD00";
                                    ctx.lineWidth = 1;
                                    ctx.moveTo(lastX,lasttrkr1YPoint);
                                    var val = trkr1YPoints[Math.floor((x-topX)/period)];
                                    ctx.lineTo(x,val+topY);
                                    lasttrkr1YPoint = val+topY;
                                    ctx.stroke();
                                    ctx.closePath();
                                }  
                                if ( trkr2YPoints.length-1 > (x-topX)/period) {
                                    ctx.beginPath();  
                                    ctx.strokeStyle = "#00DDDD";
                                    ctx.lineWidth = 1;
                                    ctx.moveTo(lastX,lasttrkr2YPoint);
                                    var val = trkr2YPoints[Math.floor((x-topX)/period)];
                                    ctx.lineTo(x,val+topY);
                                    lasttrkr2YPoint = val+topY;
                                    ctx.stroke();
                                    ctx.closePath();
                                }  
                                lastX = x;
                            }
                        } catch (error) {         
                            console.info(error.message)   
                        }
                    }
                }
            }
        }

        Component.onCompleted: {
            resetGraph()
        }
    }

    Timer {
        id: refreshTimer
        interval: timerInterval
        running: false
        repeat: true
        onTriggered: {
            try {
                var velVals;
                sampleTime = sampleTime + timerInterval;
                if ( autoConfMode < 0 ) {
                    velVals = WalkInPlaceTabController.getGraphPoses(timerInterval);
                } else {
                    var scaleSpeed = 0
                    if ( autoConfMode == 1 ) {
                        currentSampleTime = currentSampleTime + (timerInterval)
                        if ( currentSampleTime > sampleLimit ) {
                            currentSampleTime = sampleLimit
                        }
                        scaleSpeed = ((currentSampleTime / sampleLimit) * 0.5).toFixed(1)
                    } else if ( autoConfMode == 2 ) {
                        scaleSpeed = 0.5
                    } else if ( autoConfMode == 3 ) {
                        currentSampleTime = currentSampleTime + (timerInterval)
                        if ( currentSampleTime > sampleLimit ) {
                            currentSampleTime = sampleLimit
                        }
                        scaleSpeed = (((currentSampleTime / sampleLimit) * 0.5) + 0.5).toFixed(1)
                    } else if ( autoConfMode > 3 ) {
                        scaleSpeed = 1
                    }
                    velVals = WalkInPlaceTabController.trainingDataSample(scaleSpeed, timerInterval);
                }
                var velX = parseFloat(velVals[0]).toFixed(4);
                var velY = parseFloat(velVals[1]).toFixed(4);
                var velZ = parseFloat(velVals[2]).toFixed(4);            
                velX = velX > 5 ? 5 : velX;
                velY = velY > 5 ? 5 : velY;
                velZ = velZ > 5 ? 5 : velZ;
                velX = velX < -5 ? -5 : velX;
                velY = velY < -5 ? -5 : velY;
                velZ = velZ < -5 ? -5 : velZ;
                if ( Math.abs(velX) > hmdPlotMaxPoint ) {
                    hmdPlotMaxPoint = Math.abs(velX);
                    hmdPlotMinPoint = -hmdPlotMaxPoint;
                }
                if ( Math.abs(velY) > hmdPlotMaxPoint ) {
                    hmdPlotMaxPoint = Math.abs(velY);
                    hmdPlotMinPoint = -hmdPlotMaxPoint;
                }
                if ( Math.abs(velZ) > hmdPlotMaxPoint ) {
                    hmdPlotMaxPoint = Math.abs(velZ);
                    hmdPlotMinPoint = -hmdPlotMaxPoint;
                }

                velY = parseFloat(velVals[3]).toFixed(4);
                velY = velY > 10 ? 10 : velY;
                velY = velY < -10 ? -10 : velY;
                if ( Math.abs(velY) > cntrlPlotMaxPoint ) {
                    cntrlPlotMaxPoint = Math.abs(velY);
                    cntrlPlotMinPoint = -cntrlPlotMaxPoint;
                }

                velY = parseFloat(velVals[4]).toFixed(4);
                velY = velY > 10 ? 10 : velY;
                velY = velY < -10 ? -10 : velY;
                if ( Math.abs(velY) > cntrlPlotMaxPoint ) {
                    cntrlPlotMaxPoint = Math.abs(velY);
                    cntrlPlotMinPoint = -cntrlPlotMaxPoint;
                }

                velY = parseFloat(velVals[5]).toFixed(4);
                velY = velY > 10 ? 10 : velY;
                velY = velY < -10 ? -10 : velY;
                if ( Math.abs(velY) > trkrPlotMaxPoint ) {
                    trkrPlotMaxPoint = Math.abs(velY);
                    trkrPlotMinPoint = -trkrPlotMaxPoint;
                }

                velY = parseFloat(velVals[6]).toFixed(4);
                velY = velY > 10 ? 10 : velY;
                velY = velY < -10 ? -10 : velY;
                if ( Math.abs(velY) > trkrPlotMaxPoint ) {
                    trkrPlotMaxPoint = Math.abs(velY);
                    trkrPlotMinPoint = -trkrPlotMaxPoint;
                }

                var r = (hmdPlotMaxPoint - hmdPlotMinPoint)  
                var g = (hmdCanvas.rectHeight)

                var velX = parseFloat(velVals[0]).toFixed(4);
                var velY = parseFloat(velVals[1]).toFixed(4);
                var velZ = parseFloat(velVals[2]).toFixed(4);  
                velX = (((velX - hmdPlotMinPoint) * g) / r)
                velY = (((velY - hmdPlotMinPoint) * g) / r)
                velZ = (((velZ - hmdPlotMinPoint) * g) / r)
                hmdYROTVELPoints.push(velX);
                hmdYVELPoints.push(velY);
                hmdPROTVELPoints.push(velZ);
                if ( hmdYROTVELPoints.length >= hmdCanvas.seconds*hmdCanvas.sections ) {
                    hmdYROTVELPoints.shift();
                }
                if ( hmdYVELPoints.length >= hmdCanvas.seconds*hmdCanvas.sections ) {
                    hmdYVELPoints.shift();
                }
                if ( hmdPROTVELPoints.length >= hmdCanvas.seconds*hmdCanvas.sections ) {
                    hmdPROTVELPoints.shift();
                }

                r = (cntrlPlotMaxPoint - cntrlPlotMinPoint)  
                g = (cntrlCanvas.rectHeight)

                velY = parseFloat(velVals[3]).toFixed(4);
                velY = (((velY - cntrlPlotMinPoint) * g) / r)
                cntrlYPoints.push(velY);
                if ( cntrlYPoints.length >= cntrlCanvas.seconds*cntrlCanvas.sections ) {
                    cntrlYPoints.shift();
                }

                velY = parseFloat(velVals[4]).toFixed(4);
                velY = (((velY - cntrlPlotMinPoint) * g) / r)
                cntrl2YPoints.push(velY);
                if ( cntrl2YPoints.length >= cntrlCanvas.seconds*cntrlCanvas.sections ) {
                    cntrl2YPoints.shift();
                }

                r = (trkrPlotMaxPoint - trkrPlotMinPoint)  
                g = (trkrCanvas.rectHeight)

                velY = parseFloat(velVals[5]).toFixed(4);
                velY = (((velY - trkrPlotMinPoint) * g) / r)
                trkr1YPoints.push(velY);
                if ( trkr1YPoints.length >= trkrCanvas.seconds*trkrCanvas.sections ) {
                    trkr1YPoints.shift();
                }

                velY = parseFloat(velVals[6]).toFixed(4);
                velY = (((velY - trkrPlotMinPoint) * g) / r)
                trkr2YPoints.push(velY);
                if ( trkr2YPoints.length >= trkrCanvas.seconds*trkrCanvas.sections ) {
                    trkr2YPoints.shift();
                }
                   
                if ( autoConfMode >= 0 ) {
                    if ( sampleTime >= sampleLimit ) {
                        if ( autoConfMode == 0 ) {
                            autoConfMode = 1
                            sampleTime = 0
                            currentSampleTime = 0
                            autoConfigPopup.setTitle("Scaled Walking Pace Config")
                            autoConfigPopup.setTextDetail("Slowly increase your pace to jogging pace")
                            autoConfigPopup.setTimeout(3)
                            autoConfigPopup.openPopup()
                        } else if ( autoConfMode == 1) {
                            autoConfMode = 2
                            sampleTime = 0
                            currentSampleTime = 0
                            autoConfigPopup.setTitle("Jogging Pace Config")
                            autoConfigPopup.setTextDetail("Start Jogging (medium paced) IN PLACE")
                            autoConfigPopup.setTimeout(3)
                            autoConfigPopup.openPopup()
                        } else if ( autoConfMode == 2 ) {
                            autoConfMode = 3
                            sampleTime = 0
                            currentSampleTime = 0
                            autoConfigPopup.setTitle("Jogging Pace Config")
                            autoConfigPopup.setTextDetail("Slowly increase your pace to running pace")
                            autoConfigPopup.setTimeout(3)
                            autoConfigPopup.openPopup()
                        } else if ( autoConfMode == 3 ) {
                            autoConfMode = 4
                            sampleTime = 0
                            currentSampleTime = 0
                            autoConfigPopup.setTitle("Running Pace Config")
                            autoConfigPopup.setTextDetail("Start Running (fastest pace) IN PLACE")
                            autoConfigPopup.setTimeout(3)
                            autoConfigPopup.openPopup()
                        } else if ( autoConfMode == 4 ) {
                            stopTimer()
                            resetGraph()
                            autoConfMode = -2
                            sampleTime = 0
                            autoConfigPopup.setTitle("Walking Pace Config")
                            autoConfigPopup.setTextDetail("Start Walking IN PLACE in")
                            autoConfigPopup.setTimeout(5)
                            var page = mainView.pop()
                            mainView.completeTraining()                            
                        }                       
                    }
                }

                hmdCanvas.requestPaint ();
                cntrlCanvas.requestPaint ();
                trkrCanvas.requestPaint ();

            } catch (error) {  
                console.info(error.message)          
            }
        }
    }

    MyTimerPopup {
        id: autoConfigPopup
        property int profileIndex: -1
        dialogTitle: "Walking Pace Config"
        dialogText:  "Begin Walking IN PLACE in:"
        dialogTO: 5
        onClosed: {
            stopPopupTimer()
            if ( autoConfMode == 0 ) {
                startTimer()
            }
            if (cancelClicked || autoConfMode < 0 ) {
                resetGraph()
                autoConfMode = -2
                dialogTitle = "Walking Pace Config"
                dialogText = "Begin Walking (slowest pace) IN PLACE in:"
                dialogTO = 5
            } 
            sampleTime = 0
        }
        function openPopup() {
            startPopupTimer()
            open()
        }
        function setTitle(s) {
            dialogTitle = s
        }
        function setTextDetail(s) {
            dialogText = s
        }
        function setTimeout(v) {
            dialogTO = v
        }
    }


    MyInfoPopup {
        id: autoConfInfoPopup
        property int modelIndex: -1
        dialogTitle: ""
        dialogText1: "The data model creation process takes ~25 seconds 5 seconds for each phase."
        dialogText2: "The process starts with slowest to increasing to medium to medium to increasing to fast to fastest."
        dialogText3: "Do not stop moving during this process."
        dialogText4: "If you do it will be recorded and used as a valid movement model."
        dialogText5: "You can always try it, view it, and recreate the model until you have what you'd like."
        onClosed: {
            if (okClicked) {
                autoConfigPopup.openPopup()
                autoConfMode = 0
                sampleTime = 0
                currentSampleTime = 0
            } else {
                stopTimer()
                resetGraph()
                autoConfMode = -2
                sampleTime = 0
                autoConfigPopup.setTitle("Walking Pace Config")
                autoConfigPopup.setTextDetail("Start Walking IN PLACE in")
                autoConfigPopup.setTimeout(5)
                var page = mainView.pop()
            }
        }
    }

    function startAutoConf() {
        stopTimer()
        resetGraph()
        autoConfInfoPopup.open()
    }
}