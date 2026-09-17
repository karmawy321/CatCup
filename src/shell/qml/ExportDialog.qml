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
        if (exporter.state !== 1) {
            exporter.reset()
        }
        applyResolution(selectedResIndex)
    }
    // Dismissing the dialog does NOT cancel the active export
    onClosed: {}

    property int selectedResIndex: 1
    readonly property var resPresets: [
        { label: "1080p (Full HD)", h: 1080 },
        { label: "720p (HD)", h: 720 },
        { label: "4K (UHD)", h: 2160 }
    ]

    function applyResolution(index) {
        if (exporter.state === 1) return;
        selectedResIndex = index;
        var targetH = resPresets[index].h;
        var seqW = 1280, seqH = 720;
        if (session && typeof session.getSequenceAspectPreset === 'function') {
            var s = session.getSequenceAspectPreset();
            if (s === "16:9") { seqW = 16; seqH = 9; }
            else if (s === "9:16") { seqW = 9; seqH = 16; }
            else if (s === "1:1") { seqW = 1; seqH = 1; }
            else if (s === "4:5") { seqW = 4; seqH = 5; }
            else if (s === "21:9") { seqW = 21; seqH = 9; }
            else if (s.indexOf("x") !== -1) {
                var parts = s.split("x");
                if (parts.length === 2 && parseFloat(parts[1]) > 0) {
                    seqW = parseFloat(parts[0]);
                    seqH = parseFloat(parts[1]);
                }
            }
        }
        var targetW = Math.round(targetH * (seqW / seqH));
        if (targetW % 2 !== 0) targetW += 1;
        if (targetH % 2 !== 0) targetH += 1;

        if (exporter) {
            exporter.exportWidth = targetW;
            exporter.exportHeight = targetH;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 18
        spacing: 12

        // Header: "Export" and close button
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: "Export Video"
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

        // Resolution Presets Pills (Interactive)
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
                    model: root.resPresets
                    delegate: Rectangle {
                        implicitWidth: resPillText.implicitWidth + 24
                        implicitHeight: 28
                        radius: 14
                        color: root.selectedResIndex === index ? Theme.accent : (pillMa.containsMouse ? Theme.bgHover : Theme.bgElevated)
                        border.color: root.selectedResIndex === index ? Theme.accent : Theme.borderMedium
                        border.width: 1
                        opacity: exporter.state === 1 ? 0.5 : 1.0

                        Text {
                            id: resPillText
                            anchors.centerIn: parent
                            text: modelData.label
                            font.family: Theme.fontBody
                            font.pixelSize: 11
                            font.weight: root.selectedResIndex === index ? Font.DemiBold : Font.Normal
                            color: root.selectedResIndex === index ? "#000000" : Theme.textPrimary
                        }

                        MouseArea {
                            id: pillMa
                            anchors.fill: parent
                            enabled: exporter.state !== 1
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            hoverEnabled: true
                            onClicked: root.applyResolution(index)
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
                opacity: exporter.state === 1 ? 0.6 : 1.0

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
                        enabled: exporter.state !== 1
                        readOnly: exporter.state === 1
                        text: exporter.outputPath
                        onEditingFinished: exporter.setOutputPath(text)
                    }
                }

                StudioButton {
                    text: "Browse…"
                    compact: true
                    variant: "secondary"
                    enabled: exporter.state !== 1
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

        // Success Banner & Actions
        ColumnLayout {
            visible: exporter.state === 2
            Layout.fillWidth: true
            spacing: 8

            Rectangle {
                Layout.fillWidth: true
                height: 38
                radius: 6
                color: "#16342E"
                border.color: Theme.accent
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8
                    Text { text: "✓"; font.pixelSize: 14; font.bold: true; color: Theme.accent }
                    Text {
                        text: "Export completed successfully!"
                        font.family: Theme.fontBody
                        font.pixelSize: 12
                        font.bold: true
                        color: Theme.accent
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                StudioButton {
                    text: "Open Video"
                    iconText: "▶"
                    variant: "primary"
                    Layout.fillWidth: true
                    onClicked: {
                        if (exporter && typeof exporter.openCompletedFile === 'function') {
                            exporter.openCompletedFile()
                        }
                    }
                }

                StudioButton {
                    text: "Open Folder"
                    iconText: "📁"
                    variant: "secondary"
                    Layout.fillWidth: true
                    onClicked: {
                        if (exporter && typeof exporter.openCompletedFolder === 'function') {
                            exporter.openCompletedFolder()
                        }
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }

        // Action Buttons
        RowLayout {
            Layout.alignment: Qt.AlignRight
            spacing: 8

            StudioButton {
                visible: exporter.state === 2
                text: "Export Again"
                variant: "secondary"
                onClicked: exporter.reset()
            }

            // Close dialog without canceling active export
            StudioButton {
                text: exporter.state === 1 ? "Run in Background" : (exporter.state === 2 ? "Done" : "Cancel")
                variant: exporter.state === 2 ? "primary" : "ghost"
                onClicked: {
                    if (exporter.state === 2) {
                        exporter.reset()
                    }
                    root.close()
                }
            }

            // Export / Cancel Button (only shown when not in Done state)
            Rectangle {
                visible: exporter.state !== 2
                implicitWidth: 110
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
                        text: exporter.state === 1 ? "Cancel Export" : "Export"
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
                        if (exporter.state === 1) {
                            exporter.cancelExport()
                        } else {
                            var val = exporter.validateDestination(exporter.outputPath)
                            if (val === "EXISTS") {
                                overwriteConfirmDialog.open()
                            } else {
                                exporter.startExport(false)
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: overwriteConfirmDialog
        title: ""
        modal: true
        standardButtons: Dialog.NoButton
        width: 380
        height: 150
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        background: Rectangle {
            radius: 8
            color: Theme.bgSurface
            border.color: Theme.accent
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 10

            Text {
                text: "Overwrite Existing File?"
                font.family: Theme.fontBody
                font.pixelSize: 13
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }

            Text {
                text: "The destination file already exists. Do you want to overwrite it?"
                font.family: Theme.fontBody
                font.pixelSize: 11
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            Item { Layout.fillHeight: true }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: 8

                StudioButton {
                    text: "Cancel"
                    variant: "ghost"
                    onClicked: overwriteConfirmDialog.close()
                }

                StudioButton {
                    text: "Overwrite"
                    variant: "danger"
                    onClicked: {
                        overwriteConfirmDialog.close()
                        exporter.startExport(true)
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
