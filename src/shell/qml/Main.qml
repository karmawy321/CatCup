import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: root
    visible: true
    width: 1400
    height: 880
    minimumWidth: 1080
    minimumHeight: 680
    color: Theme.bgApp
    title: (session.dirty ? "● " : "") + session.projectName + " — CapCut"

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

    // ---- CapCut Top Header Bar ----
    header: Rectangle {
        id: topBar
        height: 44
        color: Theme.bgSidebar
        border.color: Theme.borderSubtle
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 12

            // Left: CapCut Wordmark & Menu Pill
            Row {
                spacing: 8
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    text: "CapCut"
                    font.family: Theme.fontBody
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    color: Theme.textPrimary
                    anchors.verticalCenter: parent.verticalCenter
                }

                // Menu Pill
                Rectangle {
                    implicitWidth: 64
                    implicitHeight: 24
                    radius: 12
                    color: menuMouse.containsMouse ? Theme.bgHover : Theme.bgElevated
                    border.color: Theme.borderMedium
                    border.width: 1
                    anchors.verticalCenter: parent.verticalCenter

                    Row {
                        anchors.centerIn: parent
                        spacing: 4
                        Text {
                            text: "Menu"
                            font.family: Theme.fontBody
                            font.pixelSize: 11
                            color: Theme.textPrimary
                        }
                        Text {
                            text: "▾"
                            font.pixelSize: 9
                            color: Theme.textSecondary
                        }
                    }

                    MouseArea {
                        id: menuMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: appMenu.open()
                    }

                    Menu {
                        id: appMenu
                        y: parent.height + 4
                        MenuItem { text: "New Project"; onTriggered: { session.newProject(); selection.clearSelection() } }
                        MenuItem { text: "Open Project… (Ctrl+O)"; onTriggered: openDialog.open() }
                        MenuItem { text: "Save (Ctrl+S)"; onTriggered: session.save() }
                        MenuItem { text: "Save As…"; onTriggered: saveDialog.open() }
                        MenuSeparator {}
                        MenuItem { text: "Export…"; onTriggered: exportDialog.open() }
                        MenuSeparator {}
                        MenuItem { text: "Undo (Ctrl+Z)"; enabled: session.canUndo; onTriggered: session.undo() }
                        MenuItem { text: "Redo (Ctrl+Y)"; enabled: session.canRedo; onTriggered: session.redo() }
                        MenuSeparator {}
                        MenuItem { text: "Quit"; onTriggered: Qt.quit() }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Center: Project Name
            Row {
                spacing: 6
                anchors.centerIn: parent

                Text {
                    text: session.projectName
                    font.family: Theme.fontBody
                    font.pixelSize: 12
                    color: Theme.textSecondary
                }
                Rectangle {
                    visible: session.dirty
                    width: 5
                    height: 5
                    radius: 2.5
                    color: Theme.warning
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Item { Layout.fillWidth: true }

            // Right: Shortcuts / Pro / Share / Export Pill Button
            RowLayout {
                spacing: 8
                anchors.verticalCenter: parent.verticalCenter

                // Pro badge
                Rectangle {
                    implicitWidth: 48
                    implicitHeight: 22
                    radius: 11
                    color: "#2C2038"
                    border.color: "#8B5CF6"
                    border.width: 1

                    Row {
                        anchors.centerIn: parent
                        spacing: 3
                        Text { text: "✦"; font.pixelSize: 10; color: "#C084FC" }
                        Text { text: "Pro"; font.family: Theme.fontBody; font.pixelSize: 10; font.bold: true; color: "#C084FC" }
                    }

                }

                // Share button
                Rectangle {
                    implicitWidth: 60
                    implicitHeight: 24
                    radius: 4
                    color: shareMouse.containsMouse ? Theme.bgHover : Theme.bgElevated
                    border.color: Theme.borderMedium
                    border.width: 1

                    Row {
                        anchors.centerIn: parent
                        spacing: 4
                        Text { text: "↗"; font.pixelSize: 10; color: Theme.textPrimary }
                        Text { text: "Share"; font.family: Theme.fontBody; font.pixelSize: 11; color: Theme.textPrimary }
                    }

                    MouseArea {
                        id: shareMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                    }
                }

                // Primary Cyan-Teal Export Pill
                Rectangle {
                    implicitWidth: 84
                    implicitHeight: 26
                    radius: 13
                    color: exportMouse.pressed ? Theme.accentPressed : (exportMouse.containsMouse ? Theme.accentHover : Theme.accent)

                    Row {
                        anchors.centerIn: parent
                        spacing: 5
                        Text {
                            text: "⇪"
                            font.pixelSize: 12
                            font.bold: true
                            color: "#000000"
                        }
                        Text {
                            text: "Export"
                            font.family: Theme.fontBody
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            color: "#000000"
                        }
                    }

                    MouseArea {
                        id: exportMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: exportDialog.open()
                    }
                }
            }
        }
    }

    // ---- Workspace Panels (Adaptive Resizing) ----
    SplitView {
        anchors.fill: parent
        orientation: Qt.Vertical

        handle: Rectangle {
            id: vHandle
            implicitHeight: 6
            color: SplitHandle.pressed ? Theme.accent : (SplitHandle.hovered ? Theme.accentHover : Theme.bgApp)

            Rectangle {
                anchors.centerIn: parent
                height: 1
                width: parent.width
                color: Theme.borderMedium
            }
        }

        SplitView {
            SplitView.fillHeight: true
            SplitView.preferredHeight: 520
            orientation: Qt.Horizontal

            handle: Rectangle {
                id: hHandle
                implicitWidth: 6
                color: SplitHandle.pressed ? Theme.accent : (SplitHandle.hovered ? Theme.accentHover : Theme.bgApp)

                Rectangle {
                    anchors.centerIn: parent
                    width: 1
                    height: parent.height
                    color: Theme.borderMedium
                }
            }


            MediaBrowser {
                SplitView.preferredWidth: 320
                SplitView.minimumWidth: 260
            }

            PreviewView {
                id: previewView
                SplitView.fillWidth: true
                SplitView.minimumWidth: 380
                playheadSec: root.playhead
            }

            Inspector {
                SplitView.preferredWidth: 300
                SplitView.minimumWidth: 240
            }
        }

        TimelineView {
            SplitView.preferredHeight: 280
            SplitView.minimumHeight: 180
            playheadSec: root.playhead
        }
    }

    // Minimal status bar footer
    footer: Rectangle {
        id: footerBar
        height: 22
        color: Theme.bgSidebar
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
                text: "Space: Play/Pause  ·  S: Split  ·  Del: Delete  ·  Scroll: Zoom"
                font.family: Theme.fontBody
                font.pixelSize: 10
                color: Theme.textTertiary
                Layout.fillWidth: true
            }

            Text {
                id: errorLabel
                font.family: Theme.fontBody
                font.pixelSize: 10
                font.weight: Font.DemiBold
                color: Theme.danger
            }

            Timer {
                id: errorTimer
                interval: 6000
                onTriggered: errorLabel.text = ""
            }
        }
    }

    FileDialog {
        id: openDialog
        title: "Open Project"
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
        title: "Save Project As"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Project (*.json)"]
        onAccepted: session.saveAs(selectedFile)
    }

    ExportDialog { id: exportDialog }
}
