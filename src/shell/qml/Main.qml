import QtQuick.Effects
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
    title: (session.dirty ? "● " : "") + session.projectName + " — CatCup"
    flags: Qt.Window | Qt.FramelessWindowHint

    property double playhead: player.playing ? player.positionSec : selection.playheadSec
    property string saveStatus: "saved"
    property string pendingAction: ""
    property string pendingOpenUrl: ""

    onClosing: function(close) {
        if (session.dirty) {
            close.accepted = false
            requestAction("quit")
        } else {
            close.accepted = true
        }
    }

    onPlayheadChanged: {
        if (!player.playing)
            previewView.preview.renderAt(playhead)
    }

    function doSave() {
        if (session.filePath === "") {
            saveDialog.open()
        } else {
            root.saveStatus = "saving"
            var ok = session.save()
            if (ok) {
                root.saveStatus = "saved"
                footerBar.info("Project saved successfully")
            } else {
                root.saveStatus = "failed"
                footerBar.error("Failed to save project")
            }
        }
    }

    function requestAction(action, openUrl) {
        root.pendingAction = action
        root.pendingOpenUrl = openUrl || ""
        if (session.dirty) {
            unsavedChangesDialog.open()
        } else {
            executePendingAction()
        }
    }

    function executePendingAction() {
        var act = root.pendingAction
        var targetUrl = root.pendingOpenUrl
        root.pendingAction = ""
        root.pendingOpenUrl = ""
        if (act === "new") {
            session.newProject()
            selection.clearSelection()
            selection.setPlayheadSec(0)
            root.saveStatus = "saved"
            footerBar.info("New project created")
        } else if (act === "open") {
            if (targetUrl !== "") {
                if (session.openFile(targetUrl)) {
                    selection.clearSelection()
                    selection.setPlayheadSec(0)
                    root.saveStatus = "saved"
                    footerBar.info("Project opened")
                } else {
                    root.saveStatus = "failed"
                    footerBar.error("Failed to open project")
                }
            }
        } else if (act === "quit") {
            Qt.quit()
        }
    }

    Timer {
        id: autosaveTimer
        interval: 3500
        repeat: false
        onTriggered: {
            if (session.dirty) {
                if (typeof session.saveRecovery === 'function') {
                    if (!session.saveRecovery()) {
                        footerBar.error("Autosave recovery snapshot failed")
                    }
                }
            }
        }
    }

    Component.onCompleted: {
        if (session && typeof session.hasRecovery === 'function' && session.hasRecovery()) {
            var recInfo = session.recoveryInfo()
            if (recInfo && recInfo.hasRecovery) {
                recoveryDialog.recoveryProjectName = recInfo.projectName || "Untitled"
                recoveryDialog.recoveryTimestamp = recInfo.timestamp || ""
                recoveryDialog.open()
            }
        }
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
        function onProjectChanged() {
            if (session.dirty) {
                root.saveStatus = "unsaved"
                autosaveTimer.restart()
            } else {
                if (root.saveStatus !== "failed") {
                    root.saveStatus = "saved"
                }
            }
        }
        function onError(msg) { footerBar.error(msg) }
        function onAutosaveFailed(reason) { footerBar.error("Autosave snapshot failed: " + reason) }
    }

    function seek(seconds) {
        var t = Math.max(0, seconds)
        if (player.durationSec > 0 && t > player.durationSec) {
            t = player.durationSec
        }
        selection.setPlayheadSec(t)
        if (player && typeof player.seekTo === 'function') {
            player.seekTo(t)
        }
        if (!player.playing) {
            previewView.preview.renderAt(t)
        }
    }

    Shortcut {
        sequence: "Space"
        onActivated: {
            if (!player.playing && player.durationSec > 0 && (player.positionSec >= player.durationSec - 0.25 || Math.abs(root.playhead - player.durationSec) < 0.25)) {
                seek(0)
            }
            player.toggle()
        }
    }
    Shortcut {
        sequence: "Left"
        onActivated: {
            if (player.playing) player.pause()
            seek(Math.max(0, root.playhead - 1.0 / 30.0))
        }
    }
    Shortcut {
        sequence: "Right"
        onActivated: {
            if (player.playing) player.pause()
            seek(root.playhead + 1.0 / 30.0)
        }
    }
    Shortcut {
        sequence: "Home"
        onActivated: {
            if (player.playing) player.pause()
            seek(0)
        }
    }
    Shortcut { sequences: [StandardKey.Undo]; onActivated: session.undo() }
    Shortcut { sequences: [StandardKey.Redo]; onActivated: session.redo() }
    Shortcut { sequence: "Ctrl+N"; onActivated: requestAction("new") }
    Shortcut { sequence: "Ctrl+S"; onActivated: doSave() }
    Shortcut { sequence: "Ctrl+Shift+S"; onActivated: saveDialog.open() }
    Shortcut { sequence: "Ctrl+O"; onActivated: openDialog.open() }
    Shortcut { sequence: "Ctrl+Q"; onActivated: requestAction("quit") }
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
        } else if (selection.selectedTransitionId && typeof session.removeTransition === 'function') {
            session.removeTransition(selection.selectedTransitionId)
            selection.clearSelection()
        }
    }

    // ---- CatCup Top Header Bar ----
    header: Rectangle {
        id: topBar
        height: 44
        color: Theme.bgSidebar
        border.color: Theme.borderSubtle
        border.width: 1

        // Window drag & double-click maximize area
        MouseArea {
            id: windowDragArea
            anchors.fill: parent
            onPressed: {
                root.startSystemMove()
            }
            onDoubleClicked: {
                if (root.visibility === Window.Maximized) {
                    root.showNormal()
                } else {
                    root.showMaximized()
                }
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 0
            spacing: 12

            // Left: CatCup Wordmark & Menu Pill
            Row {
                spacing: 8
                Layout.alignment: Qt.AlignVCenter

                Image {
                    source: "qrc:/qt/qml/NativeEditor/assets/images/catcup_icon.png"
                    width: 20
                    height: 20
                    fillMode: Image.PreserveAspectFit
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: "CatCup"
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
                        MenuItem { text: "New Project (Ctrl+N)"; onTriggered: root.requestAction("new") }
                        MenuItem { text: "Open Project… (Ctrl+O)"; onTriggered: openDialog.open() }
                        MenuItem { text: "Save (Ctrl+S)"; onTriggered: root.doSave() }
                        MenuItem { text: "Save As… (Ctrl+Shift+S)"; onTriggered: saveDialog.open() }
                        MenuSeparator {}
                        MenuItem { text: "Import CapCut Draft…"; onTriggered: mainDraftImportDialog.open() }
                        MenuItem { text: "Export CapCut Draft…"; onTriggered: mainDraftExportDialog.open() }
                        MenuSeparator {}
                        MenuItem { text: "Export…"; onTriggered: exportDialog.open() }
                        MenuSeparator {}
                        MenuItem { text: "Undo (Ctrl+Z)"; enabled: session.canUndo; onTriggered: session.undo() }
                        MenuItem { text: "Redo (Ctrl+Y)"; enabled: session.canRedo; onTriggered: session.redo() }
                        MenuSeparator {}
                        MenuItem { text: "Quit (Ctrl+Q)"; onTriggered: root.requestAction("quit") }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Center: Project Name & Clear Save Status Indicator
            Row {
                spacing: 8
                Layout.alignment: Qt.AlignVCenter

                Text {
                    text: session.projectName
                    font.family: Theme.fontBody
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    anchors.verticalCenter: parent.verticalCenter
                }

                Rectangle {
                    implicitWidth: statusPillRow.implicitWidth + 12
                    implicitHeight: 20
                    radius: 10
                    color: session.dirty ? "#2D2418" : (root.saveStatus === "failed" ? "#381E1E" : Theme.bgElevated)
                    border.color: session.dirty ? Theme.warning : (root.saveStatus === "failed" ? Theme.danger : Theme.borderSubtle)
                    border.width: 1
                    anchors.verticalCenter: parent.verticalCenter

                    Row {
                        id: statusPillRow
                        anchors.centerIn: parent
                        spacing: 4

                        Rectangle {
                            width: 6
                            height: 6
                            radius: 3
                            color: session.dirty ? Theme.warning : (root.saveStatus === "saving" ? Theme.accent : (root.saveStatus === "failed" ? Theme.danger : Theme.accent))
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: session.dirty ? "Unsaved edits" : (root.saveStatus === "saving" ? "Saving…" : (root.saveStatus === "failed" ? "Save Failed" : "Saved"))
                            font.family: Theme.fontBody
                            font.pixelSize: 10
                            color: session.dirty ? Theme.warning : (root.saveStatus === "failed" ? Theme.danger : Theme.textSecondary)
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Right: Shortcuts / Share / Export Pill Button
            RowLayout {
                spacing: 8
                Layout.alignment: Qt.AlignVCenter

                // Share button (Opens Export dialog)
                Rectangle {
                    implicitWidth: 64
                    implicitHeight: 26
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
                        onClicked: exportDialog.open()
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

            // Subtle Divider
            Rectangle {
                Layout.preferredWidth: 1
                Layout.preferredHeight: 18
                Layout.alignment: Qt.AlignVCenter
                Layout.leftMargin: 4
                Layout.rightMargin: 4
                color: Theme.borderMedium
            }

            // Window Control Buttons (Minimize, Maximize/Restore, Close)
            Row {
                Layout.alignment: Qt.AlignVCenter | Qt.AlignRight
                Layout.fillHeight: true
                spacing: 0

                // Minimize Button
                Rectangle {
                    implicitWidth: 46
                    implicitHeight: topBar.height
                    color: minMouse.pressed ? Theme.bgActive : (minMouse.containsMouse ? Theme.bgHover : "transparent")

                    Rectangle {
                        anchors.centerIn: parent
                        width: 10
                        height: 1
                        color: Theme.textSecondary
                    }

                    MouseArea {
                        id: minMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.ArrowCursor
                        onClicked: root.showMinimized()
                    }
                }

                // Maximize / Restore Button
                Rectangle {
                    implicitWidth: 46
                    implicitHeight: topBar.height
                    color: maxMouse.pressed ? Theme.bgActive : (maxMouse.containsMouse ? Theme.bgHover : "transparent")

                    Item {
                        anchors.centerIn: parent
                        width: 11
                        height: 11

                        // Single square when windowed
                        Rectangle {
                            visible: root.visibility !== Window.Maximized
                            anchors.fill: parent
                            color: "transparent"
                            border.color: Theme.textSecondary
                            border.width: 1
                        }

                        // Overlapping squares when maximized
                        Item {
                            visible: root.visibility === Window.Maximized
                            anchors.fill: parent

                            Rectangle {
                                x: 2; y: 0; width: 8; height: 8
                                color: "transparent"
                                border.color: Theme.textSecondary
                                border.width: 1
                            }
                            Rectangle {
                                x: 0; y: 2; width: 8; height: 8
                                color: maxMouse.pressed ? Theme.bgActive : (maxMouse.containsMouse ? Theme.bgHover : Theme.bgSidebar)
                                border.color: Theme.textSecondary
                                border.width: 1
                            }
                        }
                    }

                    MouseArea {
                        id: maxMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.ArrowCursor
                        onClicked: {
                            if (root.visibility === Window.Maximized) {
                                root.showNormal()
                            } else {
                                root.showMaximized()
                            }
                        }
                    }
                }

                // Close Button
                Rectangle {
                    implicitWidth: 48
                    implicitHeight: topBar.height
                    color: closeMouse.pressed ? "#c42b1c" : (closeMouse.containsMouse ? "#e81123" : "transparent")

                    Text {
                        anchors.centerIn: parent
                        text: "✕"
                        font.pixelSize: 11
                        font.family: Theme.fontBody
                        color: closeMouse.containsMouse ? "#ffffff" : Theme.textSecondary
                    }

                    MouseArea {
                        id: closeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.ArrowCursor
                        onClicked: root.close()
                    }
                }
            }
        }
    }

    // ---- Workspace Panels (Adaptive Resizing) ----
    SplitView {
        id: workspaceView
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
                id: mediaBrowserView
                SplitView.preferredWidth: 320
                SplitView.minimumWidth: 260
                mainWindow: root
            }

            PreviewView {
                id: previewView
                SplitView.fillWidth: true
                SplitView.minimumWidth: 380
                playheadSec: root.playhead
            }

            Inspector {
                id: inspectorView
                SplitView.preferredWidth: 300
                SplitView.minimumWidth: 240
                mainWindow: root
            }
        }

        TimelineView {
            SplitView.preferredHeight: 280
            SplitView.minimumHeight: 180
            playheadSec: root.playhead
            mainWindow: root
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
            statusLabel.color = Theme.danger
            statusLabel.text = msg
            statusTimer.restart()
        }

        function info(msg) {
            statusLabel.color = Theme.accent
            statusLabel.text = msg
            statusTimer.restart()
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
                id: statusLabel
                font.family: Theme.fontBody
                font.pixelSize: 10
                font.weight: Font.DemiBold
                color: Theme.danger
            }

            Timer {
                id: statusTimer
                interval: 6000
                onTriggered: statusLabel.text = ""
            }
        }
    }

    FileDialog {
        id: openDialog
        title: "Open Project"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Project (*.json)", "All files (*)"]
        onAccepted: root.requestAction("open", selectedFile)
    }

    FileDialog {
        id: saveDialog
        title: "Save Project As"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Project (*.json)"]
        onAccepted: {
            var ok = session.saveAs(selectedFile)
            if (ok) {
                root.saveStatus = "saved"
                footerBar.info("Project saved successfully")
                if (root.pendingAction !== "") {
                    root.executePendingAction()
                }
            } else {
                root.saveStatus = "failed"
                root.pendingAction = ""
                root.pendingOpenUrl = ""
                footerBar.error("Failed to save project")
            }
        }
        onRejected: {
            root.pendingAction = ""
            root.pendingOpenUrl = ""
        }
    }

    // Modal Confirmation for Unsaved Changes (New / Open / Quit)
    Dialog {
        id: unsavedChangesDialog
        title: ""
        modal: true
        closePolicy: Popup.NoAutoClose
        standardButtons: Dialog.NoButton
        width: 420
        height: 170
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        background: Rectangle {
            radius: 8
            color: Theme.bgSurface
            border.color: Theme.borderHighlight
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            Text {
                text: "Save Unsaved Changes?"
                font.family: Theme.fontBody
                font.pixelSize: 14
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }

            Text {
                text: "The project \"" + session.projectName + "\" has unsaved changes. Do you want to save them before continuing?"
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
                    onClicked: {
                        root.pendingAction = ""
                        root.pendingOpenUrl = ""
                        unsavedChangesDialog.close()
                    }
                }

                StudioButton {
                    text: "Don't Save"
                    variant: "secondary"
                    onClicked: {
                        unsavedChangesDialog.close()
                        if (session && typeof session.discardRecovery === 'function') {
                            session.discardRecovery()
                        }
                        root.executePendingAction()
                    }
                }

                StudioButton {
                    text: "Save"
                    variant: "primary"
                    onClicked: {
                        unsavedChangesDialog.close()
                        if (session.filePath === "") {
                            saveDialog.open()
                        } else {
                            var ok = session.save()
                            if (ok) {
                                root.saveStatus = "saved"
                                root.executePendingAction()
                            } else {
                                root.saveStatus = "failed"
                                root.pendingAction = ""
                                root.pendingOpenUrl = ""
                                footerBar.error("Failed to save project")
                            }
                        }
                    }
                }
            }
        }
    }

    // Modal Confirmation for Autosave Recovery
    Dialog {
        id: recoveryDialog
        title: ""
        modal: true
        standardButtons: Dialog.NoButton
        width: 460
        height: 180
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2

        property string recoveryProjectName: ""
        property string recoveryTimestamp: ""

        background: Rectangle {
            radius: 8
            color: Theme.bgSurface
            border.color: Theme.accent
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 18
            spacing: 12

            Text {
                text: "Restore Previous Project Session?"
                font.family: Theme.fontBody
                font.pixelSize: 14
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }

            Text {
                text: "An unsaved recovery snapshot (" + (recoveryDialog.recoveryTimestamp !== "" ? recoveryDialog.recoveryTimestamp : "recent") + ") for project \"" + (recoveryDialog.recoveryProjectName !== "" ? recoveryDialog.recoveryProjectName : "Untitled") + "\" was found. Would you like to restore your work?"
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
                    text: "Discard Recovery"
                    variant: "ghost"
                    onClicked: {
                        recoveryDialog.close()
                        if (session && typeof session.discardRecovery === 'function') {
                            session.discardRecovery()
                        }
                        footerBar.info("Previous recovery session discarded")
                    }
                }

                StudioButton {
                    text: "Restore Project"
                    variant: "primary"
                    onClicked: {
                        recoveryDialog.close()
                        if (session && typeof session.restoreRecovery === 'function' && session.restoreRecovery()) {
                            selection.clearSelection()
                            selection.setPlayheadSec(0)
                            root.saveStatus = "unsaved"
                            footerBar.info("Project restored from recovery backup")
                        } else {
                            footerBar.error("Could not load recovery file")
                        }
                    }
                }
            }
        }
    }

    ExportDialog { id: exportDialog }

    FileDialog {
        id: mainDraftImportDialog
        title: "Import CapCut Draft"
        fileMode: FileDialog.OpenFile
        nameFilters: ["CapCut Draft (draft_content.json)", "JSON files (*.json)", "All files (*)"]
        onAccepted: {
            var ok = session.importCapCutDraft(selectedFile)
            if (ok) {
                selection.clearSelection()
                selection.setPlayheadSec(0)
                root.saveStatus = "unsaved"
                footerBar.info("Imported CapCut Draft successfully")
            } else {
                footerBar.error("Failed to import CapCut Draft")
            }
        }
    }

    FileDialog {
        id: mainDraftExportDialog
        title: "Export CapCut Draft"
        fileMode: FileDialog.SaveFile
        nameFilters: ["CapCut Draft (draft_content.json)", "JSON files (*.json)"]
        onAccepted: {
            var ok = session.exportCapCutDraft(selectedFile)
            if (ok) {
                footerBar.info("Exported CapCut Draft successfully")
            } else {
                footerBar.error("Failed to export CapCut Draft")
            }
        }
    }

    // 1px window border when not maximized (subtle dark border for desktop contrast)
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: Theme.borderSubtle
        border.width: 1
        visible: root.visibility !== Window.Maximized
        z: 9998
    }

    // Frameless window resize borders (active only when not maximized)
    Item {
        id: resizeBorders
        anchors.fill: parent
        z: 9999
        enabled: root.visibility !== Window.Maximized

        // Top edge (excludes window controls so Close button is easily clicked)
        MouseArea {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.rightMargin: 150
            anchors.top: parent.top
            height: 5
            cursorShape: Qt.SizeVerCursor
            onPressed: root.startSystemResize(Qt.TopEdge)
        }
        // Bottom edge
        MouseArea {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 5
            cursorShape: Qt.SizeVerCursor
            onPressed: root.startSystemResize(Qt.BottomEdge)
        }
        // Left edge
        MouseArea {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: 5
            cursorShape: Qt.SizeHorCursor
            onPressed: root.startSystemResize(Qt.LeftEdge)
        }
        // Right edge
        MouseArea {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: 5
            cursorShape: Qt.SizeHorCursor
            onPressed: root.startSystemResize(Qt.RightEdge)
        }

        // Top-Left corner
        MouseArea {
            anchors.top: parent.top
            anchors.left: parent.left
            width: 8
            height: 8
            cursorShape: Qt.SizeFDiagCursor
            onPressed: root.startSystemResize(Qt.TopEdge | Qt.LeftEdge)
        }
        // Bottom-Left corner
        MouseArea {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: 8
            height: 8
            cursorShape: Qt.SizeBDiagCursor
            onPressed: root.startSystemResize(Qt.BottomEdge | Qt.LeftEdge)
        }
        // Bottom-Right corner
        MouseArea {
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: 8
            height: 8
            cursorShape: Qt.SizeFDiagCursor
            onPressed: root.startSystemResize(Qt.BottomEdge | Qt.RightEdge)
        }
    }
}
