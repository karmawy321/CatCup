import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// CapCut Desktop 1:1 Export Modal:
// - Clean matte dark zinc panel (#1E1E22)
// - Preset resolution pills (1080p, 720p, 4K)
// - Codec & format controls (H.264 MP4)
// - Destination path picker
// - Progress bar in signature cyan-teal (#00C7D4)
// - Cyan Export action button

Dialog {
    id: root
    title: ""
    modal: true
    standardButtons: Dialog.NoButton
    width: 520
    height: 420
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    background: Rectangle {
        radius: 10
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
        anchors.margins: 18
        spacing: 12

        // Header: "Export" and close button
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: "Export"
                font.family: Theme.fontBody
                font.pixelSize: 15
                font.bold: true
                color: Theme.textPrimary
                Layout.fillWidth: true
            }

            Rectangle {
                implicitWidth: 24
                implicitHeight: 24
                radius: 4
                color: closeMa.containsMouse ? Theme.bgHover : "transparent"

                Text {
                    anchors.centerIn: parent
                    text: "×"
                    font.pixelSize: 16
                    color: Theme.textSecondary
                }

                MouseArea {
                    id: closeMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.close()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.borderSubtle
        }

        // Resolution Presets Pills
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "Resolution"
                font.family: Theme.fontBody
                font.pixelSize: 11
                color: Theme.textSecondary
            }

            RowLayout {
                spacing: 8

                Repeater {
                    model: ["1080p (Full HD)", "720p (HD)", "4K (UHD)"]
                    delegate: Rectangle {
                        implicitWidth: resPillText.implicitWidth + 20
                        implicitHeight: 28
                        radius: 14
                        color: index === 0 ? Theme.accent : Theme.bgElevated
                        border.color: index === 0 ? Theme.accent : Theme.borderMedium
                        border.width: 1

                        Text {
                            id: resPillText
                            anchors.centerIn: parent
                            text: modelData
                            font.family: Theme.fontBody
                            font.pixelSize: 11
                            font.weight: index === 0 ? Font.DemiBold : Font.Normal
                            color: index === 0 ? "#000000" : Theme.textPrimary
                        }
                    }
                }
            }
        }

        // Summary Card
        Rectangle {
            Layout.fillWidth: true
            radius: 6
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
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: "Export To"
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
                    radius: 4
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
        }

        // Animated Progress Bar (Encoding)
        ColumnLayout {
            Layout.fillWidth: true
            visible: exporter.state === 1
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "Rendering & Encoding MP4 Video…"
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
            color: Theme.danger
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // Success Banner
        Rectangle {
            visible: exporter.state === 2
            Layout.fillWidth: true
            height: 36
            radius: 6
            color: "#16342E"
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
                text: "Cancel"
                variant: "ghost"
                onClicked: root.close()
            }

            // CapCut Cyan Export Pill Button
            Rectangle {
                implicitWidth: 100
                implicitHeight: 32
                radius: 16
                color: exporter.state === 1 ? "#DC2626" : (expMa.pressed ? Theme.accentPressed : (expMa.containsMouse ? Theme.accentHover : Theme.accent))

                Row {
                    anchors.centerIn: parent
                    spacing: 5
                    Text {
                        text: exporter.state === 1 ? "◼" : "⇪"
                        font.pixelSize: 12
                        font.bold: true
                        color: exporter.state === 1 ? "#FFFFFF" : "#000000"
                    }
                    Text {
                        text: exporter.state === 1 ? "Cancel" : "Export"
                        font.family: Theme.fontBody
                        font.pixelSize: 12
                        font.bold: true
                        color: exporter.state === 1 ? "#FFFFFF" : "#000000"
                    }
                }

                MouseArea {
                    id: expMa
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (exporter.state === 1)
                            exporter.cancelExport()
                        else
                            exporter.startExport()
                    }
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
