import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Bottom region: ruler + track lanes + playhead. Click selects, drag moves,
// edge-drag trims, ruler click seeks, wheel zooms. All commits go through
// undoable session commands on release — never live per-pixel edits.

Pane {
    id: root
    property double playheadSec: 0
    padding: 8
    background: Rectangle { color: "#1a1a1a" }

    readonly property real headerW: 110
    readonly property real rulerH: 26
    readonly property real px: selection.pxPerSec
    // Shared lane width: viewport width or full duration, whichever is wider.
    // The Flickable and all lanes bind to this one expression — never to each
    // other — so no width feedback loop is possible.
    readonly property real laneW: Math.max(lanesFlick.width, timeline.durationSec * root.px + 120)

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        // ---- toolbar
        RowLayout {
            spacing: 6
            Button { text: "Split (S)"; onClicked: {
                if (selection.selectedClipId !== "")
                    session.splitSelectedAtPlayhead(selection.selectedClipId, root.playheadSec)
            } }
            Button { text: "Delete"; onClicked: {
                if (selection.selectedClipId !== "") {
                    session.deleteClip(selection.selectedClipId)
                    selection.clearSelection()
                }
            } }
            Item { Layout.fillWidth: true }
            Label { text: "Zoom"; opacity: 0.6 }
            Button { text: "−"; onClicked: selection.zoomOut() }
            Button { text: "+"; onClicked: selection.zoomIn() }
            Button {
                text: "Fit"
                onClicked: {
                    var w = lanesFlick.width - 40
                    if (timeline.durationSec > 0)
                        selection.setPxPerSec(Math.max(4, Math.min(800, w / timeline.durationSec)))
                }
            }
        }

        // ---- ruler + lanes
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Column {
                width: root.headerW
                spacing: 0
                Item { width: 1; height: root.rulerH }
                Repeater {
                    model: timeline.tracks
                    delegate: Item {
                        width: root.headerW
                        height: modelData.kind === "video" ? 46 : (modelData.kind === "audio" ? 34 : 30)
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            text: modelData.name
                            font.bold: true
                        }
                    }
                }
            }

            Flickable {
                id: lanesFlick
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                // One-way sizing: content follows the lanes, never the reverse.
                contentWidth: root.laneW
                contentHeight: rulerCol.height
                flickableDirection: Flickable.HorizontalFlick

                // follow playhead during playback
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
                    if (x < lanesFlick.contentX + 40 || x > lanesFlick.contentX + lanesFlick.width - 80)
                        lanesFlick.contentX = Math.max(0, x - lanesFlick.width / 2)
                }

                MouseArea {
                    // wheel zoom anywhere over the lanes
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

                    // ruler
                    Item {
                        width: parent.width
                        height: root.rulerH
                        Repeater {
                            model: Math.ceil(timeline.durationSec) + 2
                            delegate: Item {
                                x: index * root.px
                                width: root.px
                                height: root.rulerH
                                Rectangle { width: 1; height: (index % 5 === 0) ? 14 : 7; color: "#666"; anchors.bottom: parent.bottom }
                                Label {
                                    visible: index % 5 === 0
                                    text: index + "s"
                                    font.pointSize: 7
                                    opacity: 0.7
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 14
                                    anchors.left: parent.left
                                    anchors.leftMargin: 3
                                }
                            }
                        }
                        MouseArea {
                            anchors.fill: parent
                            onPressed: (m) => selection.setPlayheadSec(Math.max(0, m.x / root.px))
                            onPositionChanged: (m) => { if (pressed) selection.setPlayheadSec(Math.max(0, m.x / root.px)) }
                        }
                    }

                    // lanes
                    Repeater {
                        model: timeline.tracks
                        delegate: Item {
                            property var track: modelData
                            width: rulerCol.width
                            height: track.kind === "video" ? 46 : (track.kind === "audio" ? 34 : 30)
                            Rectangle {
                                anchors.fill: parent
                                color: index % 2 === 0 ? "#202020" : "#1c1c1c"
                                border.color: "#2c2c2c"
                            }
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
                        }
                    }

                } // Column rulerCol (ruler + lanes only; playhead is a
                  // Flickable-level overlay below, NOT a Column child — a
                  // height-bound child would feed the Column's own implicit
                  // height back into itself: polish() loop).

                // Playhead overlay: sibling of the Column so its
                // height: rulerCol.height binding is one-way.
                Rectangle {
                    x: root.playheadSec * root.px - 1
                    y: 0
                    width: 2
                    height: rulerCol.height
                    color: "#2dd4bf"
                }
                Rectangle {
                    x: root.playheadSec * root.px - 6
                    y: 0
                    width: 12
                    height: 12
                    color: "#2dd4bf"
                }
            }
        }
    }

    // ---- one timeline clip: select / move / edge-trim
    component ClipBlock : Rectangle {
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
        width: Math.max(6, durationSec * pxPerSec - 2)
        height: laneHeight - 4
        radius: 3
        color: kind === "audio" ? "#3d5a45" : (kind === "text" ? "#5a4a6f" : "#2a5a5f")
        border.color: selected ? "#2dd4bf" : "#00000000"
        border.width: 2

        Label {
            anchors.fill: parent
            anchors.margins: 4
            text: parent.label
            elide: Text.ElideRight
            font.pointSize: 8
            verticalAlignment: Text.AlignVCenter
        }

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
                selection.select(parent.clipId)
                pressX = parent.x
                pressMouseX = m.x
                curMouseX = m.x
                pressInfo = timeline.clipInfo(parent.clipId)
                if (m.x < 8) pressedSize = "left"
                else if (m.x > width - 8) pressedSize = "right"
                else pressedSize = "move"
                drag.target = pressedSize === "move" ? parent : null
            }
            onPositionChanged: (m) => { curMouseX = m.x }
            onReleased: {
                if (pressInfo === null || pressInfo.clipId === undefined) {
                    pressedSize = ""
                    return
                }
                if (pressedSize === "move") {
                    var ns = Math.max(0, parent.x / pxPerSec)
                    parent.x = pressX // model refresh repositions authoritatively
                    session.moveClipTo(parent.clipId, ns)
                } else {
                    // Trim: the block itself doesn't move (drag.target null),
                    // so measure the pointer travel instead of item travel.
                    var dxSec = (curMouseX - pressMouseX) / pxPerSec
                    if (pressedSize === "left") {
                        session.trimClip(parent.clipId,
                            pressInfo.sourceInSec + dxSec,
                            pressInfo.sourceInSec + pressInfo.durationSec,
                            pressInfo.startSec + dxSec)
                    } else {
                        session.trimClip(parent.clipId,
                            pressInfo.sourceInSec,
                            pressInfo.sourceInSec + pressInfo.durationSec + dxSec,
                            pressInfo.startSec)
                    }
                }
                pressedSize = ""
                pressInfo = null
            }
        }
    }
}
