import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// CapCut Desktop 1:1 Multi-Track Timeline:
// - Precision Ruler with clean timecode markings (00:00, 00:03, 00:06)
// - White Laser Playhead with floating time pill
// - Multi-Track headers with [Cover] badge, Track Lock, Eye, and Mute
// - Smooth adaptive block moving (non-destructive x/y offset, snap feedback, cross-track dragging)
// - Left/Right edge-trimming grips with visual hover cues
// - CapCut slate teal clips (#1C4049) with cyan borders (#00C7D4) and audio waveforms

Rectangle {
    id: root
    property double playheadSec: 0

    color: Theme.bgApp
    border.color: Theme.borderSubtle
    border.width: 1

    readonly property real headerW: 136
    readonly property real rulerH: 32
    readonly property real px: selection.pxPerSec
    readonly property real laneW: Math.max(lanesFlick.width, timeline.durationSec * root.px + 400)

    // Active dragging state across tracks
    property string activeDragClipId: ""
    property string hoveredTrackId: ""

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- CapCut Timeline Action Toolbar ----
        Rectangle {
            Layout.fillWidth: true
            height: 38
            color: Theme.bgSurface
            border.color: Theme.borderSubtle
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 6

                // Select Arrow Tool
                Rectangle {
                    implicitWidth: 28
                    implicitHeight: 28
                    radius: 4
                    color: Theme.bgActive
                    border.color: Theme.accent
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "↖"
                        font.pixelSize: 14
                        color: Theme.accent
                    }
                }

                // Split Tool
                StudioButton {
                    text: "Split"
                    iconText: "]["
                    compact: true
                    variant: "secondary"
                    onClicked: {
                        if (selection.selectedClipId !== "")
                            session.splitSelectedAtPlayhead(selection.selectedClipId, root.playheadSec)
                    }
                }

                // Delete Tool
                StudioButton {
                    text: "Delete"
                    iconText: "×"
                    compact: true
                    variant: "ghost"
                    onClicked: {
                        if (selection.selectedClipId !== "") {
                            session.deleteClip(selection.selectedClipId)
                            selection.clearSelection()
                        } else if (selection.selectedTransitionId !== "") {
                            session.removeTransition(selection.selectedTransitionId)
                            selection.clearSelection()
                        }
                    }
                }

                // Undo
                StudioButton {
                    iconText: "↶"
                    compact: true
                    variant: "ghost"
                    enabled: session.canUndo
                    onClicked: session.undo()
                }

                // Redo
                StudioButton {
                    iconText: "↷"
                    compact: true
                    variant: "ghost"
                    enabled: session.canRedo
                    onClicked: session.redo()
                }

                Rectangle { width: 1; height: 16; color: Theme.borderMedium }

                // Auto-Magnet (CapCut Signature Toggle)
                StudioButton {
                    text: "Magnet"
                    iconText: "∩"
                    compact: true
                    checkable: true
                    checked: session.snappingEnabled
                    variant: session.snappingEnabled ? "accent" : "ghost"
                    onClicked: session.snappingEnabled = !session.snappingEnabled
                }

                // Auto-Ripple (CapCut Ripple Toggle)
                StudioButton {
                    text: "Ripple"
                    iconText: "⇥⇤"
                    compact: true
                    checkable: true
                    checked: session.rippleMode
                    variant: session.rippleMode ? "accent" : "ghost"
                    onClicked: session.rippleMode = !session.rippleMode
                }

                Item { Layout.fillWidth: true }

                // Voiceover Record Button
                Rectangle {
                    implicitWidth: 80
                    implicitHeight: 26
                    radius: 13
                    color: recMa.containsMouse ? Theme.bgHover : Theme.bgElevated
                    border.color: Theme.borderMedium
                    border.width: 1

                    Row {
                        anchors.centerIn: parent
                        spacing: 4
                        Text { text: "●"; font.pixelSize: 10; color: "#EF4444" }
                        Text {
                            text: "Record"
                            font.family: Theme.fontBody
                            font.pixelSize: 11
                            color: Theme.textSecondary
                        }
                    }


                    MouseArea {
                        id: recMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                    }
                }

                Rectangle { width: 1; height: 16; color: Theme.borderMedium }

                // CapCut Zoom Controls
                RowLayout {
                    spacing: 4

                    StudioButton {
                        text: "−"
                        compact: true
                        variant: "ghost"
                        onClicked: selection.zoomOut()
                    }

                    // Continuous Zoom Slider
                    Slider {
                        id: zoomSlider
                        from: 8
                        to: 600
                        value: root.px
                        implicitWidth: 90
                        onMoved: selection.setPxPerSec(value)
                    }

                    StudioButton {
                        text: "+"
                        compact: true
                        variant: "ghost"
                        onClicked: selection.zoomIn()
                    }

                    StudioButton {
                        text: "Fit"
                        compact: true
                        variant: "secondary"
                        onClicked: {
                            var w = lanesFlick.width - 60
                            if (timeline.durationSec > 0)
                                selection.setPxPerSec(Math.max(4, Math.min(800, w / timeline.durationSec)))
                        }
                    }
                }
            }
        }

        // ---- Multi-Track Sidebar + Ruler + Scrollable Lanes ----
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Left: CapCut Track Headers Column
            Column {
                width: root.headerW
                spacing: 0

                // Top Header Spacer with [Cover] Badge
                Rectangle {
                    width: root.headerW
                    height: root.rulerH
                    color: Theme.bgSidebar
                    border.color: Theme.borderSubtle
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8

                        // [Cover] Badge as in CapCut
                        Rectangle {
                            implicitWidth: 54
                            implicitHeight: 20
                            radius: 4
                            color: Theme.bgElevated
                            border.color: Theme.borderMedium
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "Cover"
                                font.family: Theme.fontBody
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                                color: Theme.textSecondary
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: "TRACKS"
                            font.family: Theme.fontMono
                            font.pixelSize: 8
                            font.bold: true
                            color: Theme.textTertiary
                        }
                    }
                }

                // Track Header Cards
                Repeater {
                    model: timeline.tracks
                    delegate: Rectangle {
                        width: root.headerW
                        height: modelData.kind === "video" ? 56 : (modelData.kind === "audio" ? 46 : 40)
                        color: root.hoveredTrackId === modelData.trackId ? "#202E38" : Theme.bgSidebar
                        border.color: root.hoveredTrackId === modelData.trackId ? Theme.accent : Theme.borderSubtle
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 6

                            // Track ID Badge (V1, V2, A1)
                            Rectangle {
                                implicitWidth: 26
                                implicitHeight: 22
                                radius: 4
                                color: modelData.kind === "video" ? "#1C363C" : (modelData.kind === "audio" ? "#1B2E38" : "#2E1C44")
                                border.color: modelData.kind === "video" ? Theme.accent : (modelData.kind === "audio" ? "#38BDF8" : Theme.purple)
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.name
                                    font.family: Theme.fontMono
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: modelData.kind === "video" ? Theme.accent : (modelData.kind === "audio" ? "#38BDF8" : Theme.purple)
                                }
                            }

                            Text {
                                text: modelData.kind === "video" ? "Main Video" : (modelData.kind === "audio" ? "Audio Track" : "Text Overlay")
                                font.family: Theme.fontBody
                                font.pixelSize: 10
                                color: Theme.textSecondary
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            // Track Controls: Lock, Eye, Mute
                            Row {
                                spacing: 6
                                Text { text: "⚿"; font.pixelSize: 10; color: Theme.textTertiary; opacity: 0.6 }
                                Text { text: "◉"; font.pixelSize: 10; color: Theme.textTertiary; opacity: 0.7 }
                                Text { text: "♫"; font.pixelSize: 10; color: Theme.textTertiary; opacity: 0.6 }
                            }

                        }
                    }
                }
            }

            // Right: Scrollable Ruler, Lanes & Playhead
            Flickable {
                id: lanesFlick
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: root.laneW
                contentHeight: rulerCol.height
                flickableDirection: Flickable.HorizontalFlick

                onContentWidthChanged: followPlayhead()
                Connections {
                    target: root
                    function onPlayheadSecChanged() {
                        if (player.playing)
                            followPlayhead()
                    }
                }
                function followPlayhead() {
                    var x = root.playheadSec * root.px
                    if (x < lanesFlick.contentX + 50 || x > lanesFlick.contentX + lanesFlick.width - 100)
                        lanesFlick.contentX = Math.max(0, x - lanesFlick.width / 2)
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    onWheel: (wheel) => {
                        if (wheel.angleDelta.y > 0) selection.zoomIn()
                        else selection.zoomOut()
                    }
                }

                Column {
                    id: rulerCol
                    width: root.laneW

                    // ---- CapCut Precision Ruler ----
                    Rectangle {
                        width: parent.width
                        height: root.rulerH
                        color: "#16161A"
                        border.color: Theme.borderSubtle
                        border.width: 1

                        Repeater {
                            model: Math.ceil(timeline.durationSec) + 12
                            delegate: Item {
                                x: index * root.px
                                width: root.px
                                height: root.rulerH

                                // Sub-second ticks
                                Rectangle {
                                    x: root.px * 0.25
                                    width: 1
                                    height: 3
                                    color: "#2B2B32"
                                    anchors.bottom: parent.bottom
                                }
                                Rectangle {
                                    x: root.px * 0.5
                                    width: 1
                                    height: 5
                                    color: "#383842"
                                    anchors.bottom: parent.bottom
                                }
                                Rectangle {
                                    x: root.px * 0.75
                                    width: 1
                                    height: 3
                                    color: "#2B2B32"
                                    anchors.bottom: parent.bottom
                                }

                                // Major second line
                                Rectangle {
                                    width: 1
                                    height: (index % 3 === 0) ? 12 : 7
                                    color: (index % 3 === 0) ? "#555562" : "#383842"
                                    anchors.bottom: parent.bottom
                                }

                                // CapCut Time format: 00:00, 00:03, 00:06
                                Text {
                                    visible: index % 3 === 0
                                    text: formatRulerTime(index)
                                    font.family: Theme.fontMono
                                    font.pixelSize: 9
                                    font.weight: Font.DemiBold
                                    color: "#8E8E9A"
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 14
                                    anchors.left: parent.left
                                    anchors.leftMargin: 3
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onPressed: (m) => selection.setPlayheadSec(session.snapTime(Math.max(0, m.x / root.px)))
                            onPositionChanged: (m) => {
                                if (pressed) selection.setPlayheadSec(session.snapTime(Math.max(0, m.x / root.px)))
                            }
                        }
                    }

                    // ---- Track Lanes ----
                    Repeater {
                        id: trackLanesRepeater
                        model: timeline.tracks
                        delegate: Rectangle {
                            id: laneRect
                            property var track: modelData
                            width: rulerCol.width
                            height: track.kind === "video" ? 56 : (track.kind === "audio" ? 46 : 40)
                            color: root.hoveredTrackId === track.trackId ? "#1A2830" : (index % 2 === 0 ? "#121214" : "#141417")
                            border.color: root.hoveredTrackId === track.trackId ? Theme.accent : "#222226"
                            border.width: root.hoveredTrackId === track.trackId ? 2 : 1

                            // Horizontal subtle center guideline
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                height: 1
                                color: "#1E1E22"
                            }

                            // Clips on this track
                            Repeater {
                                model: timeline
                                delegate: ClipBlock {
                                    visible: model.trackId === track.trackId
                                    clipId: model.clipId
                                    currentTrackId: track.trackId
                                    kind: model.trackKind
                                    label: model.displayText
                                    startSec: model.startSec
                                    durationSec: model.durationSec
                                    pxPerSec: root.px
                                    selected: selection.selectedClipId === model.clipId
                                    laneHeight: parent.height
                                }
                            }

                            // Transitions on this track
                            Repeater {
                                model: timeline.transitions
                                delegate: TransitionBlock {
                                    visible: modelData.trackId === track.trackId
                                    transId: modelData.id
                                    type: modelData.type
                                    startSec: modelData.startSec
                                    durationSec: modelData.rangeDurationSec
                                    pxPerSec: root.px
                                    selected: selection.selectedTransitionId === modelData.id
                                    laneHeight: parent.height
                                }
                            }
                        }
                    }
                }

                // ---- CapCut Pure White Laser Playhead ----
                // Playhead top white pill badge with current time
                Rectangle {
                    x: root.playheadSec * root.px - width / 2
                    y: 2
                    width: 52
                    height: 16
                    radius: 3
                    color: "#FFFFFF"
                    z: 50

                    Text {
                        anchors.centerIn: parent
                        text: formatPlayheadPill(root.playheadSec)
                        font.family: Theme.fontMono
                        font.pixelSize: 9
                        font.bold: true
                        color: "#000000"
                    }
                }

                // White Playhead needle line
                Rectangle {
                    x: root.playheadSec * root.px - 1
                    y: 0
                    width: 2
                    height: rulerCol.height
                    color: "#FFFFFF"
                    z: 49
                }
            }
        }
    }

    // ---- CapCut Adaptive Clip Block Component ----
    component ClipBlock : Rectangle {
        id: clipRoot
        property string clipId: ""
        property string currentTrackId: ""
        property string kind: "video"
        property string label: ""
        property double startSec: 0
        property double durationSec: 0
        property double pxPerSec: 48
        property bool selected: false
        property double laneHeight: 40

        // Adaptive dragging offset properties (Preserves QML declarative bindings!)
        property real dragOffsetX: 0
        property real dragOffsetY: 0
        property bool isDragging: false

        x: (startSec * pxPerSec) + dragOffsetX
        y: 2 + dragOffsetY
        width: Math.max(8, durationSec * pxPerSec - 2)
        height: laneHeight - 4
        radius: 4
        clip: true
        z: isDragging ? 100 : (selected ? 20 : 5)

        // CapCut Palette: Slate Cyan-Teal for Video (#1C4049), Slate Blue for Audio (#1B3248)
        color: {
            if (kind === "audio") return "#1B3248"
            if (kind === "text") return "#2D2140"
            return "#1C4049"
        }

        // CapCut Border: Bright Cyan-Teal #00C7D4 when selected
        border.color: selected ? Theme.accent : (kind === "video" ? "#2A5A66" : (kind === "audio" ? "#2B4B63" : "#4A3366"))
        border.width: selected ? 2 : 1

        // Top Filmstrip Notches for Video Clips
        Rectangle {
            visible: clipRoot.kind === "video"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 9
            color: "#33000000"

            Row {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 4
                spacing: 8
                Repeater {
                    model: Math.min(30, Math.floor(clipRoot.width / 14))
                    delegate: Rectangle {
                        width: 6
                        height: 4
                        radius: 1
                        color: "#66FFFFFF"
                    }
                }
            }
        }

        // Audio Waveform Peaks for Audio Clips
        Row {
            visible: clipRoot.kind === "audio"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            spacing: 3
            opacity: 0.65

            Repeater {
                model: Math.min(80, Math.floor(clipRoot.width / 5))
                delegate: Rectangle {
                    width: 2
                    height: Math.max(4, Math.sin(index * 1.2) * 12 + 14)
                    radius: 1
                    color: Theme.accent
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // Clip Content Label & Duration
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 4

            Text {
                text: clipRoot.label
                font.family: Theme.fontBody
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: "#FFFFFF"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Text {
                text: clipRoot.durationSec.toFixed(1) + "s"
                font.family: Theme.fontMono
                font.pixelSize: 9
                color: "#B3FFFFFF"
            }
        }

        // Selected Corner Resizing Handles
        Rectangle {
            visible: clipRoot.selected
            width: 4
            height: 10
            radius: 2
            color: "#FFFFFF"
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
        }
        Rectangle {
            visible: clipRoot.selected
            width: 4
            height: 10
            radius: 2
            color: "#FFFFFF"
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
        }

        // Left Trim Handle Grip
        Rectangle {
            id: leftGrip
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 8
            color: dragMa.mouseX < 8 ? Theme.accent : "transparent"
            opacity: 0.8
        }

        // Right Trim Handle Grip
        Rectangle {
            id: rightGrip
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 8
            color: dragMa.mouseX > parent.width - 8 ? Theme.accent : "transparent"
            opacity: 0.8
        }

        // Floating Snap/Move Badge while dragging
        Rectangle {
            visible: clipRoot.isDragging
            anchors.bottom: parent.top
            anchors.bottomMargin: 4
            anchors.horizontalCenter: parent.horizontalCenter
            implicitWidth: dragTimeText.implicitWidth + 12
            implicitHeight: 20
            radius: 4
            color: "#E600C7D4"

            Text {
                id: dragTimeText
                anchors.centerIn: parent
                text: formatPlayheadPill(Math.max(0, (clipRoot.startSec * pxPerSec + clipRoot.dragOffsetX) / pxPerSec))
                font.family: Theme.fontMono
                font.pixelSize: 9
                font.bold: true
                color: "#000000"
            }
        }

        // Interaction MouseArea for Smooth Drag-Move and Edge-Trimming
        MouseArea {
            id: dragMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: (mouseX < 8 || mouseX > width - 8) ? Qt.SizeHorCursor : Qt.ArrowCursor

            property string dragMode: "" // "move", "left_trim", "right_trim"
            property real pressStartX: 0
            property real pressMouseX: 0
            property real pressMouseY: 0
            property var pressInfo: null

            onPressed: (m) => {
                selection.select(clipRoot.clipId)
                root.activeDragClipId = clipRoot.clipId
                pressStartX = clipRoot.startSec * pxPerSec
                pressMouseX = m.x
                pressMouseY = m.y
                pressInfo = timeline.clipInfo(clipRoot.clipId)

                if (m.x < 8) {
                    dragMode = "left_trim"
                } else if (m.x > width - 8) {
                    dragMode = "right_trim"
                } else {
                    dragMode = "move"
                    clipRoot.isDragging = true
                }
            }

            onPositionChanged: (m) => {
                if (!pressed) return

                if (dragMode === "move") {
                    var dx = (m.x - pressMouseX)
                    clipRoot.dragOffsetX += dx

                    // Vertical dragging across tracks
                    var mapped = mapToItem(rulerCol, m.x, m.y)
                    var targetTrack = findTrackAtY(mapped.y)
                    if (targetTrack !== "") {
                        root.hoveredTrackId = targetTrack
                    }
                } else if (dragMode === "left_trim") {
                    var dx = (m.x - pressMouseX)
                    // Visual feedback during trim
                    clipRoot.dragOffsetX += dx
                    clipRoot.width = Math.max(8, clipRoot.width - dx)
                } else if (dragMode === "right_trim") {
                    var dx = (m.x - pressMouseX)
                    clipRoot.width = Math.max(8, clipRoot.width + dx)
                    pressMouseX = m.x
                }
            }

            onReleased: (m) => {
                clipRoot.isDragging = false
                root.activeDragClipId = ""
                var destTrack = root.hoveredTrackId
                root.hoveredTrackId = ""

                if (pressInfo === null || pressInfo.clipId === undefined) {
                    clipRoot.dragOffsetX = 0
                    clipRoot.dragOffsetY = 0
                    dragMode = ""
                    return
                }

                if (dragMode === "move") {
                    var finalPx = pressStartX + clipRoot.dragOffsetX
                    var targetSec = Math.max(0, finalPx / pxPerSec)
                    targetSec = session.snapTime(targetSec)

                    clipRoot.dragOffsetX = 0
                    clipRoot.dragOffsetY = 0

                    if (destTrack !== "" && destTrack !== clipRoot.currentTrackId) {
                        session.moveClipToTrack(clipRoot.clipId, destTrack, targetSec)
                    } else {
                        session.moveClipTo(clipRoot.clipId, targetSec)
                    }
                } else if (dragMode === "left_trim") {
                    var dxSec = clipRoot.dragOffsetX / pxPerSec
                    clipRoot.dragOffsetX = 0
                    var newStart = session.snapTime(pressInfo.startSec + dxSec)
                    var effectiveDx = newStart - pressInfo.startSec
                    session.trimClip(clipRoot.clipId,
                        pressInfo.sourceInSec + effectiveDx,
                        pressInfo.sourceInSec + pressInfo.durationSec,
                        newStart)
                } else if (dragMode === "right_trim") {
                    var newDur = Math.max(0.1, clipRoot.width / pxPerSec)
                    session.trimClip(clipRoot.clipId,
                        pressInfo.sourceInSec,
                        pressInfo.sourceInSec + newDur,
                        pressInfo.startSec)
                }

                dragMode = ""
                pressInfo = null
            }
        }
    }

    // Helper: Find Track ID by vertical Y position in rulerCol
    function findTrackAtY(y) {
        var currentY = root.rulerH
        var tracks = timeline.tracks
        for (var i = 0; i < tracks.length; ++i) {
            var t = tracks[i]
            var h = t.kind === "video" ? 56 : (t.kind === "audio" ? 46 : 40)
            if (y >= currentY && y < currentY + h) {
                return t.trackId
            }
            currentY += h
        }
        return ""
    }

    // ---- CapCut Sleek Transition Badge Component ----
    component TransitionBlock : Rectangle {
        id: transBadge
        property string transId: ""
        property string type: "crossfade"
        property double startSec: 0
        property double durationSec: 1
        property double pxPerSec: 48
        property bool selected: false
        property double laneHeight: 40

        x: startSec * pxPerSec
        y: 4
        width: Math.max(20, durationSec * pxPerSec)
        height: laneHeight - 8
        radius: 4
        color: selected ? "#0284C7" : "#0369A1"
        opacity: 0.9
        border.color: selected ? Theme.accent : "#BAE6FD"
        border.width: selected ? 2 : 1
        z: 10

        RowLayout {
            anchors.centerIn: parent
            spacing: 2

            Text {
                text: "⧖"
                font.pixelSize: 10
                color: "#FFFFFF"
            }

            Text {
                text: transBadge.type
                font.family: Theme.fontMono
                font.pixelSize: 8
                font.bold: true
                color: "#FFFFFF"
                elide: Text.ElideRight
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: selection.selectTransition(transBadge.transId)
        }
    }

    // Format ruler markings: 00:00, 00:03, 00:06
    function formatRulerTime(sec) {
        var m = Math.floor(sec / 60)
        var s = sec % 60
        return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s
    }

    // Format playhead pill: 00:00:00
    function formatPlayheadPill(sec) {
        var s = Math.max(0, sec)
        var m = Math.floor(s / 60)
        var remSec = Math.floor(s % 60)
        var frame = Math.floor((s - Math.floor(s)) * 30)
        function pad(n) { return (n < 10 ? "0" : "") + n }
        return pad(m) + ":" + pad(remSec) + ":" + pad(frame)
    }
}
