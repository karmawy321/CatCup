import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EditorCpp 1.0

// Center column: 2026 Cinema Viewport + Floating Transport Pill HUD.

Rectangle {
    id: root
    property alias preview: previewItem
    property double playheadSec: 0

    color: Theme.bgApp
    border.color: Theme.borderSubtle
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // Cinema Monitor Screen
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radiusMedium
            color: Theme.bgCanvas
            border.color: Theme.borderMedium
            border.width: 1
            clip: true

            PreviewItem {
                id: previewItem
                anchors.fill: parent
                Component.onCompleted: {
                    previewItem.setSession(session)
                    previewItem.renderAt(0)
                }
            }

            // Floating Transport Capsule HUD
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 14
                implicitWidth: transportRow.implicitWidth + 24
                implicitHeight: 42
                radius: 21
                color: "#E613161F"
                border.color: Theme.borderMedium
                border.width: 1

                RowLayout {
                    id: transportRow
                    anchors.centerIn: parent
                    spacing: 10

                    // Rewind to start
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

                    // Step back
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

                    // Play / Pause glowing circular toggle
                    Rectangle {
                        implicitWidth: 32
                        implicitHeight: 32
                        radius: 16
                        color: player.playing ? Theme.accent : Theme.cyan

                        Text {
                            anchors.centerIn: parent
                            text: player.playing ? "❚❚" : "▶"
                            font.pixelSize: 13
                            font.bold: true
                            color: "#090A0D"
                        }

                        MouseArea {
                            anchors.fill: parent
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

                    // Stop
                    StudioButton {
                        iconText: "■"
                        compact: true
                        variant: "ghost"
                        onClicked: player.stop()
                    }

                    Rectangle { width: 1; height: 18; color: Theme.borderMedium }

                    // Digital Time Readout
                    Text {
                        text: fmt(root.playheadSec) + " / " + fmt(player.durationSec)
                        font.family: Theme.fontMono
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }
                }
            }

            // Top-Right Resolution Chip
            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 10
                implicitWidth: resText.implicitWidth + 12
                implicitHeight: 22
                radius: 11
                color: "#B313161F"
                border.color: Theme.borderSubtle
                border.width: 1

                Text {
                    id: resText
                    anchors.centerIn: parent
                    text: "720p · Real-Time Compositor"
                    font.family: Theme.fontMono
                    font.pixelSize: 9
                    color: Theme.textSecondary
                }
            }
        }
    }

    function fmt(sec) {
        var s = Math.max(0, sec)
        var m = Math.floor(s / 60)
        var r = (s - m * 60).toFixed(2)
        return m + ":" + (r < 10 ? "0" : "") + r
    }
}
