import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import EditorCpp 1.0

// Center column: live preview surface + transport + timecode.

Pane {
    id: root
    property alias preview: previewItem
    property double playheadSec: 0
    padding: 8

    background: Rectangle { color: "#141414" }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        PreviewItem {
            id: previewItem
            Layout.fillWidth: true
            Layout.fillHeight: true
            Component.onCompleted: {
                previewItem.setSession(session)
                previewItem.renderAt(0)
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button { text: player.playing ? "❚❚" : "▶"; onClicked: player.toggle() }
            Button { text: "■"; onClicked: player.stop() }
            Button {
                text: "|◀"
                onClicked: {
                    player.pause()
                    selection.setPlayheadSec(0)
                    previewItem.renderAt(0)
                }
            }

            Label {
                text: fmt(root.playheadSec) + " / " + fmt(player.durationSec)
                font.family: "Consolas"
            }

            Item { Layout.fillWidth: true }

            Label { text: "720p preview · CPU (S1)"; opacity: 0.5 }
        }
    }

    function fmt(sec) {
        var s = Math.max(0, sec)
        var m = Math.floor(s / 60)
        var r = (s - m * 60).toFixed(2)
        return m + ":" + (r < 10 ? "0" : "") + r
    }
}
