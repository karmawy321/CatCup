import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// 2026 Studio Export Modal Dialog: Output summary, destination picker,
// animated progress indicator, status, cancel, and start actions.

Dialog {
    id: root
    title: ""
    modal: true
    standardButtons: Dialog.NoButton
    width: 500
    height: 360
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    background: Rectangle {
        radius: Theme.radiusLarge
        color: Theme.bgSurface
        border.color: Theme.borderHighlight
        border.width: 1
    }

    onOpened: {
        exporter.setOutputPath(exporter.outputPath)
    }
    onClosed: {
        if (exporter.state === 1)
            exporter.cancelExport()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        // Modal Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Rectangle {
                implicitWidth: 32
                implicitHeight: 32
                radius: 8
                color: "#162822"
                border.color: "#00E59966"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "⇪"
                    font.pixelSize: 16
                    color: Theme.accent
                }
            }

            ColumnLayout {
                spacing: 1
                Text {
                    text: "Export Master Video"
                    font.family: Theme.fontBody
                    font.pixelSize: 14
                    font.bold: true
                    color: Theme.textPrimary
                }
                Text {
                    text: "H.264 / AAC MP4 Render Pipeline"
                    font.family: Theme.fontBody
                    font.pixelSize: 10
                    color: Theme.textTertiary
                }
            }
        }

        // Summary Card
        Rectangle {
            Layout.fillWidth: true
            radius: Theme.radiusMedium
            color: Theme.bgCard
            border.color: Theme.borderMedium
            border.width: 1
            implicitHeight: summaryText.implicitHeight + 16

            Text {
                id: summaryText
                anchors.fill: parent
                anchors.margins: 10
                text: exporter.summary
                font.family: Theme.fontMono
                font.pixelSize: 10
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
        }

        // Destination Path Picker
        Text {
            text: "Destination Path"
            font.family: Theme.fontBody
            font.pixelSize: 11
            color: Theme.textSecondary
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Rectangle {
                Layout.fillWidth: true
                height: 32
                radius: Theme.radiusSmall
                color: Theme.bgElevated
                border.color: pathInput.activeFocus ? Theme.borderFocus : Theme.borderMedium
                border.width: 1

                TextInput {
                    id: pathInput
                    anchors.fill: parent
                    anchors.margins: 6
                    font.family: Theme.fontMono
                    font.pixelSize: 11
                    color: Theme.textPrimary
                    selectByMouse: true
                    text: exporter.outputPath
                    onEditingFinished: exporter.setOutputPath(text)
                }
            }

            StudioButton {
                text: "Browse…"
                compact: true
                variant: "secondary"
                onClicked: saveDialog.open()
            }
        }

        // Animated Progress Bar
        ColumnLayout {
            Layout.fillWidth: true
            visible: exporter.state === 1
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "Encoding Video Frames…"
                    font.family: Theme.fontBody
                    font.pixelSize: 11
                    color: Theme.textSecondary
                    Layout.fillWidth: true
                }
                Text {
                    text: Math.round(exporter.progress * 100) + "%"
                    font.family: Theme.fontMono
                    font.pixelSize: 11
                    font.bold: true
                    color: Theme.accent
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 6
                radius: 3
                color: Theme.bgApp

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    radius: 3
                    width: Math.max(0, Math.min(parent.width, parent.width * exporter.progress))
                    color: Theme.accent
                }
            }
        }

        // Error Message
        Text {
            visible: exporter.errorText !== ""
            text: exporter.errorText
            font.family: Theme.fontBody
            font.pixelSize: 11
            font.bold: true
            color: Theme.red
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // Success Banner
        Rectangle {
            visible: exporter.state === 2
            Layout.fillWidth: true
            height: 36
            radius: Theme.radiusSmall
            color: "#14281E"
            border.color: Theme.accent
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6
                Text { text: "✓"; font.pixelSize: 12; color: Theme.accent }
                Text {
                    text: "Export completed successfully!"
                    font.family: Theme.fontBody
                    font.pixelSize: 11
                    font.bold: true
                    color: Theme.accent
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        Item { Layout.fillHeight: true }

        // Action Buttons
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8

            StudioButton {
                text: "Close"
                variant: "ghost"
                onClicked: root.close()
            }

            StudioButton {
                text: exporter.state === 1 ? "Cancel Export" : "Start Export"
                iconText: exporter.state === 1 ? "◼" : "⇪"
                variant: exporter.state === 1 ? "danger" : "primary"
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
