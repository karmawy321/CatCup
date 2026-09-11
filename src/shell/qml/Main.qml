import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: root
    visible: true
    width: 1360
    height: 860
    minimumWidth: 1080
    minimumHeight: 680
    color: Theme.bgApp
    title: (session.dirty ? "● " : "") + session.projectName + " — CatCup Studio 2026"

    property double playhead: player.playing ? player.positionSec : selection.playheadSec

    onPlayheadChanged: {
        if (!player.playing)
            previewView.preview.renderAt(playhead)
    }

    Connections {
        target: player
        function onPositionChanged() {
            if (player.playing)
                selection.setPlayheadSec(player.positionSec)
        }
        function onVideoFrame(image, seconds) {
            previewView.preview.showImage(image, seconds)
        }
        function onPlaybackFinished() {
            selection.setPlayheadSec(player.positionSec)
            previewView.preview.renderAt(player.positionSec)
        }
        function onError(msg) { footerBar.error(msg) }
    }
    Connections {
        target: session
        function onError(msg) { footerBar.error(msg) }
    }

    Shortcut { sequence: "Space"; onActivated: player.toggle() }
    Shortcut { sequences: [StandardKey.Undo]; onActivated: session.undo() }
    Shortcut { sequences: [StandardKey.Redo]; onActivated: session.redo() }
    Shortcut { sequence: "Ctrl+S"; onActivated: session.save() }
    Shortcut { sequence: "Ctrl+O"; onActivated: openDialog.open() }
    Shortcut { sequence: "S"; onActivated: doSplit() }
    Shortcut { sequence: "Delete"; onActivated: doDelete() }

    function doSplit() {
        if (selection.selectedClipId !== "")
            session.splitSelectedAtPlayhead(selection.selectedClipId, root.playhead)
    }
    function doDelete() {
        if (selection.selectedClipId !== "") {
            session.deleteClip(selection.selectedClipId)
            selection.clearSelection()
        } else if (selection.selectedTransitionId !== "") {
            session.removeTransition(selection.selectedTransitionId)
            selection.clearSelection()
        }
    }

    // ---- 2026 Studio Header & HUD Bar ----
    header: Rectangle {
        id: studioHeader
        height: 48
        color: Theme.bgSurface
        border.color: Theme.borderSubtle
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 10

            // Brand Badge
            Rectangle {
                implicitWidth: 82
                implicitHeight: 28
                radius: 6
                color: "#162822"
                border.color: "#00E59966"
                border.width: 1

                Row {
                    anchors.centerIn: parent
                    spacing: 4
                    Text {
                        text: "CAT"
                        font.family: Theme.fontBody
                        font.pixelSize: 12
                        font.weight: Font.Black
                        color: Theme.textPrimary
                    }
                    Text {
                        text: "CUP"
                        font.family: Theme.fontBody
                        font.pixelSize: 12
                        font.weight: Font.Black
                        color: Theme.accent
                    }
                }
            }

            // Project Name & Status
            Row {
                spacing: 6
                Text {
                    text: session.projectName
                    font.family: Theme.fontBody
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    anchors.verticalCenter: parent.verticalCenter
                }
                Rectangle {
                    width: 6
                    height: 6
                    radius: 3
                    color: session.dirty ? Theme.orange : Theme.accent
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    text: session.dirty ? "Unsaved" : "Saved"
                    font.family: Theme.fontBody
                    font.pixelSize: 10
                    color: session.dirty ? Theme.orange : Theme.textTertiary
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            // File & Edit Studio Menus
            Row {
                spacing: 2
                StudioButton {
                    text: "File"
                    compact: true
                    variant: "ghost"
                    onClicked: fileMenu.open()
                    Menu {
                        id: fileMenu
                        y: parent.height + 2
                        MenuItem { text: "New Project"; onTriggered: { session.newProject(); selection.clearSelection() } }
                        MenuItem { text: "Open Project… (Ctrl+O)"; onTriggered: openDialog.open() }
                        MenuItem { text: "Save Project (Ctrl+S)"; onTriggered: session.save() }
                        MenuItem { text: "Save Project As…"; onTriggered: saveDialog.open() }
                        MenuSeparator {}
                        MenuItem { text: "Export Video…"; onTriggered: exportDialog.open() }
                        MenuSeparator {}
                        MenuItem { text: "Quit"; onTriggered: Qt.quit() }
                    }
                }
                StudioButton {
                    text: "Edit"
                    compact: true
                    variant: "ghost"
                    onClicked: editMenu.open()
                    Menu {
                        id: editMenu
                        y: parent.height + 2
                        MenuItem { text: "Undo (Ctrl+Z)"; enabled: session.canUndo; onTriggered: session.undo() }
                        MenuItem { text: "Redo (Ctrl+Y)"; enabled: session.canRedo; onTriggered: session.redo() }
                        MenuSeparator {}
                        MenuItem { text: "Split at Playhead (S)"; onTriggered: doSplit() }
                        MenuItem { text: "Delete Selected"; onTriggered: doDelete() }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Center HUD: Glowing Timecode & Undo/Redo/Pills
            RowLayout {
                spacing: 6

                TimecodeDisplay {
                    seconds: root.playhead
                    totalSeconds: player.durationSec
                }

                StudioButton {
                    iconText: "↶"
                    compact: true
                    variant: "ghost"
                    enabled: session.canUndo
                    onClicked: session.undo()
                }

                StudioButton {
                    iconText: "↷"
                    compact: true
                    variant: "ghost"
                    enabled: session.canRedo
                    onClicked: session.redo()
                }

                Rectangle { width: 1; height: 16; color: Theme.borderMedium }

                StudioButton {
                    text: "SNAP"
                    compact: true
                    variant: session.snappingEnabled ? "accent" : "ghost"
                    checked: session.snappingEnabled
                    onClicked: session.snappingEnabled = !session.snappingEnabled
                }

                StudioButton {
                    text: "RIPPLE"
                    compact: true
                    variant: session.rippleMode ? "accent" : "ghost"
                    checked: session.rippleMode
                    onClicked: session.rippleMode = !session.rippleMode
                }
            }

            Item { Layout.fillWidth: true }

            // Right: Format chip & Glowing Export Action
            Rectangle {
                implicitWidth: 86
                implicitHeight: 24
                radius: 12
                color: Theme.bgElevated
                border.color: Theme.borderSubtle
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "1080p · 30fps"
                    font.family: Theme.fontMono
                    font.pixelSize: 10
                    color: Theme.textSecondary
                }
            }

            StudioButton {
                text: "Export"
                iconText: "⇪"
                variant: "primary"
                onClicked: exportDialog.open()
            }
        }
    }

    // ---- Minimal Status Footer ----
    footer: Rectangle {
        id: footerBar
        height: 26
        color: Theme.bgSurface
        border.color: Theme.borderSubtle
        border.width: 1

        function error(msg) {
            errorLabel.text = msg
            errorTimer.restart()
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12

            Text {
                text: "Space: Play/Pause  ·  S: Split  ·  Del: Delete  ·  Scroll: Zoom Timeline"
                font.family: Theme.fontBody
                font.pixelSize: 11
                color: Theme.textTertiary
                Layout.fillWidth: true
            }

            Text {
                id: errorLabel
                font.family: Theme.fontBody
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: Theme.red
            }

            Timer {
                id: errorTimer
                interval: 6000
                onTriggered: errorLabel.text = ""
            }
        }
    }

    // ---- Workspace Split Panels ----
    SplitView {
        anchors.fill: parent
        orientation: Qt.Vertical

        handle: Rectangle {
            implicitHeight: 3
            color: Theme.bgApp
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: 1
                color: SplitHandle.hovered ? Theme.accent : Theme.borderSubtle
            }
        }

        SplitView {
            SplitView.fillHeight: true
            SplitView.preferredHeight: 530
            orientation: Qt.Horizontal

            handle: Rectangle {
                implicitWidth: 3
                color: Theme.bgApp
                Rectangle {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 1
                    color: SplitHandle.hovered ? Theme.accent : Theme.borderSubtle
                }
            }

            MediaBrowser {
                SplitView.preferredWidth: 300
                SplitView.minimumWidth: 240
            }

            PreviewView {
                id: previewView
                SplitView.fillWidth: true
                SplitView.minimumWidth: 360
                playheadSec: root.playhead
            }

            Inspector {
                SplitView.preferredWidth: 320
                SplitView.minimumWidth: 260
            }
        }

        TimelineView {
            SplitView.preferredHeight: 260
            SplitView.minimumHeight: 160
            playheadSec: root.playhead
        }
    }

    FileDialog {
        id: openDialog
        title: "Open project"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Project (*.json)", "All files (*)"]
        onAccepted: {
            if (session.openFile(selectedFile)) {
                selection.clearSelection()
                selection.setPlayheadSec(0)
            }
        }
    }

    FileDialog {
        id: saveDialog
        title: "Save project as"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Project (*.json)"]
        onAccepted: session.saveAs(selectedFile)
    }

    ExportDialog { id: exportDialog }
}
