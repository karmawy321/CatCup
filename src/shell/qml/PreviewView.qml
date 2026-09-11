import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EditorCpp 1.0

// CapCut Desktop 1:1 Player Viewport:
// - Top header "Player - Timeline 01"
// - Dark 16:9 Cinema Compositor viewport
// - CapCut bottom transport bar with cyan-teal timecode, centered Play/Pause,
//   Ratio switcher (16:9, 9:16 TikTok, 1:1), Full quality pill, and fullscreen toggle.

Rectangle {
    id: root
    property alias preview: previewItem
    property double playheadSec: 0

    color: Theme.bgApp
    border.color: Theme.borderSubtle
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- CapCut Player Header ----
        Rectangle {
            Layout.fillWidth: true
            height: 34
            color: Theme.bgSurface
            border.color: Theme.borderSubtle
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text {
                    text: "Player - Timeline 01"
                    font.family: Theme.fontBody
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Item { Layout.fillWidth: true }

                // Layout & display options icon
                Rectangle {
                    implicitWidth: 24
                    implicitHeight: 24
                    radius: 4
                    color: optMa.containsMouse ? Theme.bgHover : "transparent"

                    Row {
                        anchors.centerIn: parent
                        spacing: 2
                        Repeater {
                            model: 3
                            delegate: Rectangle {
                                width: 3
                                height: 3
                                radius: 1.5
                                color: Theme.textTertiary
                            }
                        }
                    }

                    MouseArea {
                        id: optMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }
        }

        // ---- Cinema Monitor Viewport ----
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Theme.bgCanvas
            clip: true

            PreviewItem {
                id: previewItem
                anchors.fill: parent
                Component.onCompleted: {
                    previewItem.setSession(session)
                    previewItem.renderAt(0)
                }
            }

            // Top-right aspect ratio indicator badge
            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 10
                implicitWidth: aspectBadgeText.implicitWidth + 12
                implicitHeight: 22
                radius: 4
                color: "#CC121214"
                border.color: Theme.borderSubtle
                border.width: 1

                Text {
                    id: aspectBadgeText
                    anchors.centerIn: parent
                    text: session ? session.getSequenceAspectPreset() : "16:9"
                    font.family: Theme.fontMono
                    font.pixelSize: 10
                    font.bold: true
                    color: Theme.accent
                }
            }
        }

        // ---- CapCut Transport Control Bar ----
        Rectangle {
            Layout.fillWidth: true
            height: 42
            color: Theme.bgSurface
            border.color: Theme.borderSubtle
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 10

                // Left: Cyan Timecode readout (Current / Total)
                RowLayout {
                    spacing: 4

                    Text {
                        text: formatTimecode(root.playheadSec)
                        font.family: Theme.fontMono
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: Theme.accent
                    }

                    Text {
                        text: "/"
                        font.family: Theme.fontMono
                        font.pixelSize: 11
                        color: Theme.textTertiary
                    }

                    Text {
                        text: formatTimecode(player.durationSec)
                        font.family: Theme.fontMono
                        font.pixelSize: 11
                        color: Theme.textSecondary
                    }
                }

                // Step backward / rewind buttons
                StudioButton {
                    iconText: "|◀"
                    compact: true
                    variant: "ghost"
                    onClicked: {
                        player.pause()
                        selection.setPlayheadSec(0)
                        previewItem.renderAt(0)
                    }
                }

                StudioButton {
                    iconText: "◀"
                    compact: true
                    variant: "ghost"
                    onClicked: {
                        var target = Math.max(0, root.playheadSec - 1.0 / 30.0)
                        selection.setPlayheadSec(target)
                        previewItem.renderAt(target)
                    }
                }

                Item { Layout.fillWidth: true }

                // Center: Solid Play / Pause Toggle Button
                Rectangle {
                    implicitWidth: 32
                    implicitHeight: 32
                    radius: 16
                    color: playBtnMa.pressed ? Theme.bgActive : (playBtnMa.containsMouse ? Theme.bgHover : Theme.bgElevated)
                    border.color: Theme.borderMedium
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: player.playing ? "❚❚" : "▶"
                        font.pixelSize: 12
                        font.bold: true
                        color: "#FFFFFF"
                    }

                    MouseArea {
                        id: playBtnMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: player.toggle()
                    }
                }

                // Step forward
                StudioButton {
                    iconText: "▶"
                    compact: true
                    variant: "ghost"
                    onClicked: {
                        var target = root.playheadSec + 1.0 / 30.0
                        selection.setPlayheadSec(target)
                        previewItem.renderAt(target)
                    }
                }

                Item { Layout.fillWidth: true }

                // Right Controls: Quality pill, Ratio dropdown, and Fullscreen icon
                // Quality pill
                Rectangle {
                    implicitWidth: 46
                    implicitHeight: 24
                    radius: 4
                    color: Theme.bgElevated
                    border.color: Theme.borderSubtle
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "Full ▾"
                        font.family: Theme.fontBody
                        font.pixelSize: 10
                        color: Theme.textSecondary
                    }
                }

                // Ratio preset switcher dropdown button
                Rectangle {
                    id: ratioBtn
                    implicitWidth: 64
                    implicitHeight: 24
                    radius: 4
                    color: ratioMa.containsMouse ? Theme.bgHover : Theme.bgElevated
                    border.color: ratioMenu.visible ? Theme.accent : Theme.borderSubtle
                    border.width: 1

                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            text: (session ? session.getSequenceAspectPreset() : "16:9") + " ▾"
                            font.family: Theme.fontBody
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }
                    }

                    MouseArea {
                        id: ratioMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: ratioMenu.open()
                    }

                    Menu {
                        id: ratioMenu
                        y: -height - 4

                        MenuItem {
                            text: "16:9 (Landscape / YouTube)"
                            onTriggered: {
                                session.setSequenceAspectPreset("16:9")
                                previewItem.renderAt(root.playheadSec)
                            }
                        }
                        MenuItem {
                            text: "9:16 (TikTok / Shorts / Reels)"
                            onTriggered: {
                                session.setSequenceAspectPreset("9:16")
                                previewItem.renderAt(root.playheadSec)
                            }
                        }
                        MenuItem {
                            text: "1:1 (Square / Instagram)"
                            onTriggered: {
                                session.setSequenceAspectPreset("1:1")
                                previewItem.renderAt(root.playheadSec)
                            }
                        }
                        MenuItem {
                            text: "4:3 (Classic TV)"
                            onTriggered: {
                                session.setSequenceAspectPreset("4:3")
                                previewItem.renderAt(root.playheadSec)
                            }
                        }
                        MenuItem {
                            text: "21:9 (Ultrawide Cinema)"
                            onTriggered: {
                                session.setSequenceAspectPreset("21:9")
                                previewItem.renderAt(root.playheadSec)
                            }
                        }
                    }
                }

                // Fullscreen button
                Rectangle {
                    implicitWidth: 26
                    implicitHeight: 24
                    radius: 4
                    color: fsMa.containsMouse ? Theme.bgHover : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "⛶"
                        font.pixelSize: 13
                        color: Theme.textPrimary
                    }

                    MouseArea {
                        id: fsMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                    }
                }
            }
        }
    }

    // Timecode formatter: HH:MM:SS:FF
    function formatTimecode(sec, fps) {
        if (fps === undefined || fps <= 0) fps = 30
        var s = Math.max(0, sec)
        var totalFrames = Math.floor(s * fps)
        var f = totalFrames % fps
        var totalSeconds = Math.floor(s)
        var secPart = totalSeconds % 60
        var totalMinutes = Math.floor(totalSeconds / 60)
        var minPart = totalMinutes % 60
        var hours = Math.floor(totalMinutes / 60)

        function pad(n) { return (n < 10 ? "0" : "") + n }
        return pad(hours) + ":" + pad(minPart) + ":" + pad(secPart) + ":" + pad(f)
    }
}
