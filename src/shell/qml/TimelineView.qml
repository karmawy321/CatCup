import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Bottom region: 2026 Studio Multi-Track Timeline with Precision Ruler,
// Laser Playhead, Simulated Audio Waveforms, Filmstrip Clip Headers, and
// Track Sidebars.

Rectangle {
    id: root
    property double playheadSec: 0

    color: Theme.bgApp
    border.color: Theme.borderSubtle
    border.width: 1

    readonly property real headerW: 130
    readonly property real rulerH: 30
    readonly property real px: selection.pxPerSec
    readonly property real laneW: Math.max(lanesFlick.width, timeline.durationSec * root.px + 200)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        // ---- Modern Timeline Toolbar ----
        Rectangle {
            Layout.fillWidth: true
            height: 36
            radius: Theme.radiusSmall
            color: Theme.bgSurface
            border.color: Theme.borderSubtle
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                StudioButton {
                    text: "Split"
                    iconText: "✂"
                    compact: true
                    variant: "secondary"
                    onClicked: {
                        if (selection.selectedClipId !== "")
                            session.splitSelectedAtPlayhead(selection.selectedClipId, root.playheadSec)
                    }
                }

                StudioButton {
                    text: "Delete"
                    iconText: "🗑"
                    compact: true
                    variant: "danger"
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

                Rectangle { width: 1; height: 16; color: Theme.borderMedium }

                StudioButton {
                    text: "Snap"
                    iconText: "🧲"
                    compact: true
                    checkable: true
                    checked: session.snappingEnabled
                    variant: session.snappingEnabled ? "accent" : "ghost"
                    onClicked: session.snappingEnabled = !session.snappingEnabled
                }

                StudioButton {
                    text: "Ripple"
                    iconText: "🌊"
                    compact: true
                    checkable: true
                    checked: session.rippleMode
                    variant: session.rippleMode ? "accent" : "ghost"
                    onClicked: session.rippleMode = !session.rippleMode
                }

                Item { Layout.fillWidth: true }

                // Zoom Controls
                RowLayout {
                    spacing: 4

                    Text {
                        text: "Zoom:"
                        font.family: Theme.fontBody
                        font.pixelSize: 10
                        color: Theme.textTertiary
                    }

                    StudioButton {
                        text: "−"
                        compact: true
                        variant: "ghost"
                        onClicked: selection.zoomOut()
                    }

                    Rectangle {
                        implicitWidth: 44
                        implicitHeight: 22
                        radius: 4
                        color: Theme.bgElevated
                        border.color: Theme.borderSubtle
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: Math.round(root.px) + "px"
                            font.family: Theme.fontMono
                            font.pixelSize: 10
                            color: Theme.textSecondary
                        }
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

        // ---- Ruler + Track Headers + Lanes ----
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Left: Track Headers Sidebar
            Column {
                width: root.headerW
                spacing: 0

                // Empty corner spacer opposite the ruler
                Rectangle {
                    width: root.headerW
                    height: root.rulerH
                    color: Theme.bgSurface
                    border.color: Theme.borderSubtle
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "TRACKS"
                        font.family: Theme.fontMono
                        font.pixelSize: 9
                        font.bold: true
                        color: Theme.textTertiary
                    }
                }

                // Track Header Cards
                Repeater {
                    model: timeline.tracks
                    delegate: Rectangle {
                        width: root.headerW
                        height: modelData.kind === "video" ? 54 : (modelData.kind === "audio" ? 44 : 38)
                        color: Theme.bgSurface
                        border.color: Theme.borderSubtle
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            spacing: 6

                            // Kind Badge
                            Rectangle {
                                implicitWidth: 26
                                implicitHeight: 22
                                radius: 4
                                color: modelData.kind === "video" ? "#163836" : (modelData.kind === "audio" ? "#183820" : "#2E1C44")
                                border.color: modelData.kind === "video" ? Theme.cyan : (modelData.kind === "audio" ? Theme.accent : Theme.purple)
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.name
                                    font.family: Theme.fontMono
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: modelData.kind === "video" ? Theme.cyan : (modelData.kind === "audio" ? Theme.accent : Theme.purple)
                                }
                            }

                            Text {
                                text: modelData.kind.toUpperCase()
                                font.family: Theme.fontBody
                                font.pixelSize: 10
                                color: Theme.textSecondary
                                Layout.fillWidth: true
                            }

                            // Track Controls (Mute / Lock status icons)
                            Text {
                                text: "🔒"
                                font.pixelSize: 10
                                color: Theme.textTertiary
                                opacity: 0.6
                            }
                        }
                    }
                }
            }

            // Right: Scrollable Lanes & Playhead
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

                    // ---- Modern Precision Ruler ----
                    Rectangle {
                        width: parent.width
                        height: root.rulerH
                        color: "#0D0F15"
                        border.color: Theme.borderSubtle
                        border.width: 1

                        Repeater {
                            model: Math.ceil(timeline.durationSec) + 4
                            delegate: Item {
                                x: index * root.px
                                width: root.px
                                height: root.rulerH

                                // Sub-second ticks
                                Rectangle {
                                    x: root.px * 0.25
                                    width: 1
                                    height: 4
                                    color: "#252B3A"
                                    anchors.bottom: parent.bottom
                                }
                                Rectangle {
                                    x: root.px * 0.5
                                    width: 1
                                    height: 6
                                    color: "#353D52"
                                    anchors.bottom: parent.bottom
                                }
                                Rectangle {
                                    x: root.px * 0.75
                                    width: 1
                                    height: 4
                                    color: "#252B3A"
                                    anchors.bottom: parent.bottom
                                }

                                // Major second line
                                Rectangle {
                                    width: 1
                                    height: (index % 5 === 0) ? 14 : 9
                                    color: (index % 5 === 0) ? Theme.cyan : "#4A5570"
                                    anchors.bottom: parent.bottom
                                }

                                Text {
                                    visible: index % 5 === 0
                                    text: index + "s"
                                    font.family: Theme.fontMono
                                    font.pixelSize: 9
                                    font.bold: true
                                    color: Theme.cyan
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 15
                                    anchors.left: parent.left
                                    anchors.leftMargin: 4
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
                        model: timeline.tracks
                        delegate: Rectangle {
                            property var track: modelData
                            width: rulerCol.width
                            height: track.kind === "video" ? 54 : (track.kind === "audio" ? 44 : 38)
                            color: index % 2 === 0 ? "#11141C" : "#0E1117"
                            border.color: "#1A1F2C"
                            border.width: 1

                            // Horizontal subtle center alignment guideline
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                height: 1
                                color: "#161B26"
                            }

                            // Clips on this track
                            Repeater {
                                model: timeline
                                delegate: ClipBlock {
                                    visible: model.trackId === track.trackId
                                    clipId: model.clipId
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

                // ---- Laser Playhead Needle & Line ----
                // Playhead needle header (diamond head)
                Rectangle {
                    x: root.playheadSec * root.px - 7
                    y: 2
                    width: 14
                    height: 14
                    radius: 2
                    rotation: 45
                    color: Theme.cyan
                    border.color: "#FFFFFF"
                    border.width: 1
                    z: 50
                }

                // Playhead glowing laser beam extending across all lanes
                Rectangle {
                    x: root.playheadSec * root.px - 1
                    y: 0
                    width: 2
                    height: rulerCol.height
                    color: Theme.cyan
                    z: 49

                    // Subtle cyan glow halo
                    Rectangle {
                        anchors.centerIn: parent
                        width: 6
                        height: parent.height
                        color: Theme.cyanGlow
                    }
                }
            }
        }
    }

    // ---- 2026 Sleek Clip Block Component ----
    component ClipBlock : Rectangle {
        id: clipRoot
        property string clipId: ""
        property string kind: "video"
        property string label: ""
        property double startSec: 0
        property double durationSec: 0
        property double pxPerSec: 48
        property bool selected: false
        property double laneHeight: 40

        x: startSec * pxPerSec
        y: 2
        width: Math.max(8, durationSec * pxPerSec - 2)
        height: laneHeight - 4
        radius: Theme.radiusSmall
        clip: true

        // Base gradient per kind
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: kind === "audio" ? Theme.trackAudioGrad : (kind === "text" ? Theme.trackTextGrad : Theme.trackVideoGrad)
            }
            GradientStop {
                position: 1.0
                color: kind === "audio" ? Theme.trackAudio : (kind === "text" ? Theme.trackText : Theme.trackVideo)
            }
        }

        border.color: selected ? Theme.accent : Theme.borderHighlight
        border.width: selected ? 2 : 1

        // Top Filmstrip Header for Video Clips
        Rectangle {
            visible: clipRoot.kind === "video"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 10
            color: "#33000000"

            Row {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 4
                spacing: 8
                Repeater {
                    model: Math.min(20, Math.floor(clipRoot.width / 12))
                    delegate: Rectangle {
                        width: 5
                        height: 4
                        radius: 1
                        color: "#66FFFFFF"
                    }
                }
            }
        }

        // Simulated Audio Waveforms for Audio Clips
        Row {
            visible: clipRoot.kind === "audio"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 6
            anchors.rightMargin: 6
            spacing: 3
            opacity: 0.55

            Repeater {
                model: Math.min(60, Math.floor(clipRoot.width / 5))
                delegate: Rectangle {
                    width: 2
                    // Deterministic pseudorandom heights simulating an audio waveform
                    height: Math.max(4, Math.sin(index * 1.3) * 12 + 14)
                    radius: 1
                    color: Theme.accent
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // Clip Content Label & Kind Icon
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 4

            Text {
                text: clipRoot.kind === "video" ? "🎬" : (clipRoot.kind === "audio" ? "🎵" : "💬")
                font.pixelSize: 10
            }

            Text {
                text: clipRoot.label
                font.family: Theme.fontBody
                font.pixelSize: 10
                font.weight: Font.DemiBold
                color: "#FFFFFF"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            Text {
                text: clipRoot.durationSec.toFixed(1) + "s"
                font.family: Theme.fontMono
                font.pixelSize: 8
                color: "#CCFFFFFF"
            }
        }

        // Left Trim Handle Grip
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 6
            color: drag.mouseX < 8 ? Theme.accent : "transparent"
            opacity: 0.8
        }

        // Right Trim Handle Grip
        Rectangle {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 6
            color: drag.mouseX > parent.width - 8 ? Theme.accent : "transparent"
            opacity: 0.8
        }

        // Interaction MouseArea for Drag-Move and Edge-Trimming
        MouseArea {
            id: drag
            anchors.fill: parent
            hoverEnabled: true
            drag.target: parent
            drag.axis: Drag.XAxis
            drag.minimumX: 0
            cursorShape: (mouseX < 8 || mouseX > width - 8) ? Qt.SizeHorCursor : Qt.ArrowCursor

            property string pressedSize: ""
            property real pressX: 0
            property real pressMouseX: 0
            property real curMouseX: 0
            property var pressInfo: null

            onPressed: (m) => {
                selection.select(clipRoot.clipId)
                pressX = clipRoot.x
                pressMouseX = m.x
                curMouseX = m.x
                pressInfo = timeline.clipInfo(clipRoot.clipId)
                if (m.x < 8) pressedSize = "left"
                else if (m.x > width - 8) pressedSize = "right"
                else pressedSize = "move"
                drag.target = pressedSize === "move" ? clipRoot : null
            }
            onPositionChanged: (m) => { curMouseX = m.x }
            onReleased: {
                if (pressInfo === null || pressInfo.clipId === undefined) {
                    pressedSize = ""
                    return
                }
                if (pressedSize === "move") {
                    var ns = Math.max(0, clipRoot.x / pxPerSec)
                    ns = session.snapTime(ns)
                    clipRoot.x = pressX
                    session.moveClipTo(clipRoot.clipId, ns)
                } else {
                    var dxSec = (curMouseX - pressMouseX) / pxPerSec
                    if (pressedSize === "left") {
                        var newStart = session.snapTime(pressInfo.startSec + dxSec)
                        var effectiveDx = newStart - pressInfo.startSec
                        session.trimClip(clipRoot.clipId,
                            pressInfo.sourceInSec + effectiveDx,
                            pressInfo.sourceInSec + pressInfo.durationSec,
                            newStart)
                    } else {
                        var newEnd = session.snapTime(pressInfo.startSec + pressInfo.durationSec + dxSec)
                        var newDur = Math.max(0.1, newEnd - pressInfo.startSec)
                        session.trimClip(clipRoot.clipId,
                            pressInfo.sourceInSec,
                            pressInfo.sourceInSec + newDur,
                            pressInfo.startSec)
                    }
                }
                pressedSize = ""
                pressInfo = null
            }
        }
    }

    // ---- 2026 Sleek Transition Badge Component ----
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
        radius: Theme.radiusSmall
        color: selected ? "#0284C7" : "#0369A1"
        opacity: 0.9
        border.color: selected ? Theme.cyan : "#BAE6FD"
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
}
