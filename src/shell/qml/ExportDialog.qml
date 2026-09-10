import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// Export dialog: output summary, destination, progress, cancel, failures.
// Long work runs on a worker thread — the editor never freezes.

Dialog {
    id: root
    title: "Export"
    modal: true
    standardButtons: Dialog.NoButton
    width: 460
    height: 320
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    onOpened: {
        exporter.setOutputPath(exporter.outputPath) // refresh binding
    }
    onClosed: {
        if (exporter.state === 1) // Running
            exporter.cancelExport()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        Label {
            text: exporter.summary
            font.family: "Consolas"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: pathField
                Layout.fillWidth: true
                text: exporter.outputPath
                onEditingFinished: exporter.setOutputPath(text)
            }
            Button {
                text: "Browse…"
                onClicked: saveDialog.open()
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: 1
            value: exporter.progress
            visible: exporter.state === 1
        }
        Label {
            text: Math.round(exporter.progress * 100) + "%"
            visible: exporter.state === 1
        }
        Label {
            text: exporter.errorText
            color: "#e0655f"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            visible: exporter.errorText !== ""
        }
        Label {
            text: "Export complete: " + exporter.outputPath
            color: "#7fbf7f"
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            visible: exporter.state === 2
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button {
                text: "Cancel"
                onClicked: root.close()
            }
            Button {
                text: exporter.state === 1 ? "Stop" : "Export"
                highlighted: true
                onClicked: {
                    if (exporter.state === 1)
                        exporter.cancelExport()
                    else
                        exporter.startExport()
                }
            }
        }
    }

    FileDialog {
        id: saveDialog
        title: "Export destination"
        fileMode: FileDialog.SaveFile
        nameFilters: ["MP4 (*.mp4)"]
        onAccepted: exporter.setOutputUrl(selectedFile)
    }
}
