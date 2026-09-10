import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 800
    minimumWidth: 1024
    minimumHeight: 640
    title: (session.dirty ? "● " : "") + session.projectName + " — native_editor"

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
    Shortcut { sequence: "S"; onActivated: doSplit() }

    function doSplit() {
        if (selection.selectedClipId !== "")
            session.splitSelectedAtPlayhead(selection.selectedClipId, root.playhead)
    }
    function doDelete() {
        if (selection.selectedClipId !== "") {
            session.deleteClip(selection.selectedClipId)
            selection.clearSelection()
        }
    }

    menuBar: MenuBar {
        Menu {
            title: "File"
            Action { text: "New"; onTriggered: { session.newProject(); selection.clearSelection() } }
            Action { text: "Open…"; onTriggered: openDialog.open() }
            Action { text: "Save"; onTriggered: session.save() }
            Action { text: "Save As…"; onTriggered: saveDialog.open() }
            MenuSeparator {}
            Action { text: "Export…"; onTriggered: exportDialog.open() }
            MenuSeparator {}
            Action { text: "Quit"; onTriggered: Qt.quit() }
        }
        Menu {
            title: "Edit"
            Action { text: "Undo"; enabled: session.canUndo; onTriggered: session.undo() }
            Action { text: "Redo"; enabled: session.canRedo; onTriggered: session.redo() }
            MenuSeparator {}
            Action { text: "Split at playhead (S)"; onTriggered: doSplit() }
            Action { text: "Delete selection"; onTriggered: doDelete() }
        }
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            Label { text: session.projectName; font.bold: true; Layout.fillWidth: true }
            Label {
                text: session.dirty ? "unsaved changes" : "saved"
                color: session.dirty ? "#e0a75f" : "#7fbf7f"
            }
            ToolButton { text: "Undo"; enabled: session.canUndo; onClicked: session.undo() }
            ToolButton { text: "Redo"; enabled: session.canRedo; onClicked: session.redo() }
            Button {
                text: "Export…"
                highlighted: true
                onClicked: exportDialog.open()
            }
        }
    }

    footer: footerBar

    Frame {
        id: footerBar
        function error(msg) {
            errorLabel.text = msg
            errorTimer.restart()
        }
        RowLayout {
            anchors.fill: parent
            Label { text: "Space play/pause · S split · wheel zooms timeline" ; opacity: 0.6; Layout.fillWidth: true }
            Label { id: errorLabel; color: "#e0655f" }
            Timer { id: errorTimer; interval: 6000; onTriggered: errorLabel.text = "" }
        }
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Vertical

        SplitView {
            SplitView.fillHeight: true
            SplitView.preferredHeight: 520
            orientation: Qt.Horizontal

            MediaBrowser {
                SplitView.preferredWidth: 280
                SplitView.minimumWidth: 200
            }
            PreviewView {
                id: previewView
                SplitView.fillWidth: true
                SplitView.minimumWidth: 320
                playheadSec: root.playhead
            }
            Inspector {
                SplitView.preferredWidth: 300
                SplitView.minimumWidth: 220
            }
        }

        TimelineView {
            SplitView.preferredHeight: 240
            SplitView.minimumHeight: 140
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
