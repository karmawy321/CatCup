import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// Left column: CapCut Asset Browser with horizontal icon-tabs,
// sub-sidebar navigation, media cards with 'Added' chips, and AI tools.

Rectangle {
    id: root
    color: Theme.bgSidebar
    border.color: Theme.borderSubtle
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- 1. CapCut Icon-Tabs Header ----
        Rectangle {
            Layout.fillWidth: true
            height: 48
            color: Theme.bgSidebar
            border.color: Theme.borderSubtle
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                spacing: 2

                Repeater {
                    model: [
                        { name: "Media", icon: "◫" },
                        { name: "Audio", icon: "♫" },
                        { name: "Text", icon: "T" },
                        { name: "Effects", icon: "✦" },
                        { name: "Transitions", icon: "⧖" },
                        { name: "Captions", icon: "CC" },
                        { name: "Smart", icon: "★" }
                    ]


                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "transparent"

                        Column {
                            anchors.centerIn: parent
                            spacing: 1

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.icon
                                font.pixelSize: 13
                                font.bold: true
                                color: tabStack.currentIndex === index ? Theme.accent : (tabMouse.containsMouse ? Theme.textPrimary : Theme.textTertiary)
                            }

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.name
                                font.family: Theme.fontBody
                                font.pixelSize: 10
                                color: tabStack.currentIndex === index ? Theme.accent : (tabMouse.containsMouse ? Theme.textPrimary : Theme.textTertiary)
                            }
                        }

                        // Bottom Cyan active indicator line
                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: 2
                            color: tabStack.currentIndex === index ? Theme.accent : "transparent"
                        }

                        MouseArea {
                            id: tabMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: tabStack.currentIndex = index
                        }
                    }
                }
            }
        }

        // ---- 2. Main Content Split: Sub-sidebar + Content Area ----
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Sub-sidebar (Left Rail)
            Rectangle {
                Layout.preferredWidth: 80
                Layout.fillHeight: true
                color: Theme.bgSubSidebar
                border.color: Theme.borderSubtle
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: 8
                    spacing: 4

                    Text {
                        text: "Import"
                        font.family: Theme.fontBody
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        color: Theme.textTertiary
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 26
                        radius: 4
                        color: subNavMedia.containsMouse ? Theme.bgHover : Theme.bgElevated

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            text: "Media"
                            font.family: Theme.fontBody
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            color: Theme.accent
                        }

                        MouseArea {
                            id: subNavMedia
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: tabStack.currentIndex = 0
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 26
                        radius: 4
                        color: "transparent"

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            text: "Smart AI"
                            font.family: Theme.fontBody
                            font.pixelSize: 11
                            color: Theme.textSecondary
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: tabStack.currentIndex = 6
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            // Content Area Stack
            StackLayout {
                id: tabStack
                currentIndex: 0
                Layout.fillWidth: true
                Layout.fillHeight: true

                // ==================== TAB 0: MEDIA ====================
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    // Header Row: + Import Pill & Search
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        // + Import Pill Button
                        Rectangle {
                            implicitWidth: 84
                            implicitHeight: 26
                            radius: 13
                            color: importMouse.containsMouse ? Theme.bgHover : Theme.bgElevated
                            border.color: Theme.borderMedium
                            border.width: 1

                            Row {
                                anchors.centerIn: parent
                                spacing: 4
                                Text { text: "+"; font.pixelSize: 12; font.bold: true; color: Theme.accent }
                                Text { text: "Import"; font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textPrimary }
                            }

                            MouseArea {
                                id: importMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: importDialog.open()
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // Search box
                        Rectangle {
                            implicitWidth: 100
                            implicitHeight: 24
                            radius: 4
                            color: Theme.bgCard
                            border.color: searchInput.activeFocus ? Theme.borderFocus : Theme.borderMedium
                            border.width: 1

                            TextInput {
                                id: searchInput
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 6
                                font.family: Theme.fontBody
                                font.pixelSize: 10
                                color: Theme.textPrimary
                                selectByMouse: true

                                Text {
                                    anchors.fill: parent
                                    text: "Search…"
                                    font.family: Theme.fontBody
                                    font.pixelSize: 10
                                    color: Theme.textTertiary
                                    visible: parent.text === "" && !parent.activeFocus
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }
                    }

                    // "All" label
                    Text {
                        text: "All"
                        font.family: Theme.fontBody
                        font.pixelSize: 10
                        color: Theme.textTertiary
                    }

                    // Media Cards List (CapCut Style)
                    ListView {
                        id: mediaList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 8
                        model: library

                        delegate: ColumnLayout {
                            width: mediaList.width
                            visible: assetName.toLowerCase().indexOf(searchInput.text.toLowerCase()) >= 0
                            height: visible ? 100 : 0
                            spacing: 4

                            // Card Frame
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 74
                                radius: 4
                                color: ListView.isCurrentItem ? Theme.bgActive : (cardMouse.containsMouse ? Theme.bgHover : Theme.bgCard)
                                border.color: ListView.isCurrentItem ? Theme.borderFocus : Theme.borderMedium
                                border.width: 1
                                clip: true

                                Image {
                                    anchors.fill: parent
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    cache: true
                                    source: "image://thumb/" + assetId
                                }

                                // Top-Left "Added" Chip (Exact CapCut style)
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.margins: 4
                                    radius: 2
                                    color: "#CC18181C"
                                    implicitWidth: addedText.implicitWidth + 8
                                    implicitHeight: addedText.implicitHeight + 2

                                    Text {
                                        id: addedText
                                        anchors.centerIn: parent
                                        text: "Added"
                                        font.family: Theme.fontBody
                                        font.pixelSize: 8
                                        color: "#FFFFFF"
                                    }
                                }

                                // Bottom-Right Duration Chip
                                Rectangle {
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 4
                                    radius: 2
                                    color: "#CC000000"
                                    implicitWidth: durText.implicitWidth + 6
                                    implicitHeight: durText.implicitHeight + 2

                                    Text {
                                        id: durText
                                        anchors.centerIn: parent
                                        text: durationSec.toFixed(1) + "s"
                                        font.family: Theme.fontMono
                                        font.pixelSize: 8
                                        color: "#FFFFFF"
                                    }
                                }

                                MouseArea {
                                    id: cardMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: mediaList.currentIndex = index
                                    onDoubleClicked: addCurrent()
                                }
                            }

                            // Asset File Name Label
                            Text {
                                text: assetName
                                font.family: Theme.fontBody
                                font.pixelSize: 10
                                color: Theme.textSecondary
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                        }
                    }

                    // Bottom: Add to timeline button
                    Rectangle {
                        Layout.fillWidth: true
                        height: 28
                        radius: 4
                        color: addMouse.containsMouse ? Theme.accentHover : Theme.accent
                        opacity: mediaList.currentIndex >= 0 ? 1.0 : 0.4

                        Text {
                            anchors.centerIn: parent
                            text: "+ Add to Timeline"
                            font.family: Theme.fontBody
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            color: "#000000"
                        }

                        MouseArea {
                            id: addMouse
                            anchors.fill: parent
                            enabled: mediaList.currentIndex >= 0
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: addCurrent()
                        }
                    }
                }

                // ==================== TAB 1: AUDIO ====================
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    Text { text: "Audio Library"; font.family: Theme.fontBody; font.pixelSize: 12; font.bold: true; color: Theme.textPrimary }
                    Text { text: "Import audio tracks, sound effects, or background music."; font.family: Theme.fontBody; font.pixelSize: 10; color: Theme.textTertiary; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                    StudioButton {
                        text: "Import Audio File"
                        iconText: "+"
                        variant: "primary"
                        Layout.fillWidth: true
                        onClicked: importDialog.open()
                    }

                    Item { Layout.fillHeight: true }
                }

                // ==================== TAB 2: TEXT ====================
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    Text { text: "Title Text"; font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textSecondary }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 32
                        radius: 4
                        color: Theme.bgCard
                        border.color: titleInput.activeFocus ? Theme.borderFocus : Theme.borderMedium
                        border.width: 1

                        TextInput {
                            id: titleInput
                            anchors.fill: parent
                            anchors.margins: 6
                            font.family: Theme.fontBody
                            font.pixelSize: 12
                            color: Theme.textPrimary
                            selectByMouse: true
                            text: "Default Title"
                        }
                    }

                    StudioButton {
                        text: "+ Add Default Text"
                        variant: "primary"
                        Layout.fillWidth: true
                        onClicked: {
                            var id = session.addTitle(titleInput.text)
                            if (id !== "") selection.select(id)
                        }
                    }

                    Item { Layout.fillHeight: true }
                }

                // ==================== TAB 3: EFFECTS ====================
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    ListView {
                        id: fxList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 6
                        model: ListModel {
                            ListElement { name: "Vignette"; typeName: "vignette"; desc: "Soft edge shading" }
                            ListElement { name: "Box Blur"; typeName: "blur"; desc: "Gaussian blur filter" }
                            ListElement { name: "Sharpen"; typeName: "sharpen"; desc: "Detail enhancement" }
                            ListElement { name: "Chroma Key"; typeName: "chroma_key"; desc: "Green screen removal" }
                            ListElement { name: "Warm Cinema"; typeName: "preset_warm"; desc: "Golden hour tones" }
                            ListElement { name: "Cool Film"; typeName: "preset_cool"; desc: "Modern teal film look" }
                            ListElement { name: "Black & White"; typeName: "preset_bw"; desc: "Classic monochrome" }
                            ListElement { name: "Vibrant Punch"; typeName: "preset_vibrant"; desc: "High saturation pop" }
                        }

                        delegate: Rectangle {
                            width: fxList.width
                            height: 44
                            radius: 4
                            color: ListView.isCurrentItem ? Theme.bgActive : (fxM.containsMouse ? Theme.bgHover : Theme.bgCard)
                            border.color: ListView.isCurrentItem ? Theme.borderFocus : Theme.borderMedium
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 8

                                Text { text: "✦"; font.pixelSize: 12; color: Theme.accent }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Text { text: name; font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textPrimary }
                                    Text { text: desc; font.family: Theme.fontBody; font.pixelSize: 9; color: Theme.textTertiary; elide: Text.ElideRight; Layout.fillWidth: true }
                                }
                            }

                            MouseArea {
                                id: fxM
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: fxList.currentIndex = index
                                onDoubleClicked: applyFx()
                            }
                        }
                    }

                    StudioButton {
                        text: "Apply Effect"
                        variant: "primary"
                        Layout.fillWidth: true
                        enabled: fxList.currentIndex >= 0 && selection.selectedClipId !== ""
                        onClicked: applyFx()
                    }
                }

                // ==================== TAB 4: TRANSITIONS ====================
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    ListView {
                        id: transList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 6
                        model: ListModel {
                            ListElement { name: "Crossfade"; typeName: "crossfade"; desc: "Smooth linear dissolve" }
                            ListElement { name: "Dip to Black"; typeName: "dip_black"; desc: "Fade through black" }
                            ListElement { name: "Dip to White"; typeName: "dip_white"; desc: "Flash through white" }
                            ListElement { name: "Wipe Left"; typeName: "wipe_left"; desc: "Slide left transition" }
                            ListElement { name: "Wipe Right"; typeName: "wipe_right"; desc: "Slide right transition" }
                            ListElement { name: "Wipe Up"; typeName: "wipe_up"; desc: "Slide up transition" }
                            ListElement { name: "Wipe Down"; typeName: "wipe_down"; desc: "Slide down transition" }
                        }

                        delegate: Rectangle {
                            width: transList.width
                            height: 44
                            radius: 4
                            color: ListView.isCurrentItem ? Theme.bgActive : (trM.containsMouse ? Theme.bgHover : Theme.bgCard)
                            border.color: ListView.isCurrentItem ? Theme.borderFocus : Theme.borderMedium
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 8

                                Text { text: "⧖"; font.pixelSize: 12; color: Theme.accent }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Text { text: name; font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textPrimary }
                                    Text { text: desc; font.family: Theme.fontBody; font.pixelSize: 9; color: Theme.textTertiary }
                                }
                            }

                            MouseArea {
                                id: trM
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: transList.currentIndex = index
                                onDoubleClicked: applyTrans()
                            }
                        }
                    }

                    StudioButton {
                        text: "Apply Transition"
                        variant: "primary"
                        Layout.fillWidth: true
                        enabled: transList.currentIndex >= 0 && selection.selectedClipId !== ""
                        onClicked: applyTrans()
                    }
                }

                // ==================== TAB 5: CAPTIONS ====================
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    Text { text: "Auto Captions"; font.family: Theme.fontBody; font.pixelSize: 12; font.bold: true; color: Theme.textPrimary }
                    Text { text: "Generate timed subtitle captions automatically from transcript."; font.family: Theme.fontBody; font.pixelSize: 10; color: Theme.textTertiary; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 70
                        radius: 4
                        color: Theme.bgCard
                        border.color: Theme.borderMedium
                        border.width: 1

                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: 4
                            clip: true
                            TextArea {
                                id: transInput
                                font.family: Theme.fontBody
                                font.pixelSize: 11
                                color: Theme.textPrimary
                                wrapMode: Text.WordWrap
                                placeholderText: "Paste transcript text here…"
                            }
                        }
                    }

                    StudioButton {
                        text: "Generate Captions"
                        variant: "primary"
                        Layout.fillWidth: true
                        onClicked: {
                            if (transInput.text !== "") session.generateAutoCaptions(transInput.text, 4)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        StudioButton {
                            text: "Import SRT"
                            variant: "secondary"
                            Layout.fillWidth: true
                            onClicked: srtImportDialog.open()
                        }
                        StudioButton {
                            text: "Export SRT"
                            variant: "secondary"
                            Layout.fillWidth: true
                            onClicked: srtExportDialog.open()
                        }
                    }

                    Item { Layout.fillHeight: true }
                }

                // ==================== TAB 6: SMART AI ====================
                ScrollView {
                    contentWidth: availableWidth
                    clip: true

                    ColumnLayout {
                        width: parent.width
                        spacing: 10

                        // Silence Cut
                        Rectangle {
                            Layout.fillWidth: true
                            radius: 4
                            color: Theme.bgCard
                            border.color: Theme.borderMedium
                            border.width: 1
                            implicitHeight: silCol.implicitHeight + 16

                            ColumnLayout {
                                id: silCol
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                Text { text: "Smart Silence Cut"; font.family: Theme.fontBody; font.pixelSize: 11; font.bold: true; color: Theme.textPrimary }
                                Text { text: "Cuts silent segments and ripples audible audio together."; font.family: Theme.fontBody; font.pixelSize: 9; color: Theme.textTertiary; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                                StudioButton {
                                    text: "Cut Silence (Ripple)"
                                    variant: "primary"
                                    Layout.fillWidth: true
                                    enabled: selection.selectedClipId !== ""
                                    onClicked: session.autoSilenceCut(selection.selectedClipId, -35.0, 0.4)
                                }
                            }
                        }

                        // Scene Split
                        Rectangle {
                            Layout.fillWidth: true
                            radius: 4
                            color: Theme.bgCard
                            border.color: Theme.borderMedium
                            border.width: 1
                            implicitHeight: scnCol.implicitHeight + 16

                            ColumnLayout {
                                id: scnCol
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                Text { text: "Scene Cut Splitter"; font.family: Theme.fontBody; font.pixelSize: 11; font.bold: true; color: Theme.textPrimary }
                                Text { text: "Splits video clip at detected scene transitions."; font.family: Theme.fontBody; font.pixelSize: 9; color: Theme.textTertiary; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                                StudioButton {
                                    text: "Split at Scene Cuts"
                                    variant: "primary"
                                    Layout.fillWidth: true
                                    enabled: selection.selectedClipId !== ""
                                    onClicked: session.autoSceneSplit(selection.selectedClipId, 0.25)
                                }
                            }
                        }

                        // Loudness Normalize
                        Rectangle {
                            Layout.fillWidth: true
                            radius: 4
                            color: Theme.bgCard
                            border.color: Theme.borderMedium
                            border.width: 1
                            implicitHeight: lufsCol.implicitHeight + 16

                            ColumnLayout {
                                id: lufsCol
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8

                                Text { text: "Loudness Normalization"; font.family: Theme.fontBody; font.pixelSize: 11; font.bold: true; color: Theme.textPrimary }
                                Text { text: "Normalizes audio to -14 LUFS standard with peak limiting."; font.family: Theme.fontBody; font.pixelSize: 9; color: Theme.textTertiary; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                                StudioButton {
                                    text: "Normalize (-14 LUFS)"
                                    variant: "primary"
                                    Layout.fillWidth: true
                                    enabled: selection.selectedClipId !== ""
                                    onClicked: session.normalizeClipAudio(selection.selectedClipId, -14.0)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    function addCurrent() {
        if (mediaList.currentIndex < 0) return
        var id = session.addClipToTimeline(library.assetIdAt(mediaList.currentIndex))
        if (id !== "") selection.select(id)
    }

    function applyFx() {
        if (fxList.currentIndex < 0 || selection.selectedClipId === "") return
        var item = fxList.model.get(fxList.currentIndex)
        if (item.typeName === "preset_warm") session.setClipColorAdjust(selection.selectedClipId, 0.05, 1.15, 1.1, 0.3, 0.05)
        else if (item.typeName === "preset_cool") session.setClipColorAdjust(selection.selectedClipId, 0.0, 1.15, 0.95, -0.3, -0.05)
        else if (item.typeName === "preset_bw") session.setClipColorAdjust(selection.selectedClipId, 0.0, 1.2, 0.0, 0.0, 0.0)
        else if (item.typeName === "preset_vibrant") session.setClipColorAdjust(selection.selectedClipId, 0.05, 1.2, 1.45, 0.05, 0.0)
        else session.addClipEffect(selection.selectedClipId, item.typeName)
    }

    function applyTrans() {
        if (transList.currentIndex < 0 || selection.selectedClipId === "") return
        var item = transList.model.get(transList.currentIndex)
        var clipInfo = timeline.clipInfo(selection.selectedClipId)
        if (!clipInfo || !clipInfo.trackId) return
        var nextClip = timeline.adjacentClipId(selection.selectedClipId, true)
        var prevClip = timeline.adjacentClipId(selection.selectedClipId, false)
        var fromId = selection.selectedClipId
        var toId = nextClip
        if (toId === "" && prevClip !== "") { fromId = prevClip; toId = selection.selectedClipId }
        var transId = session.addTransition(clipInfo.trackId, fromId, toId, item.typeName, 1.0, 0)
        if (transId !== "") selection.selectTransition(transId)
    }

    FileDialog {
        id: importDialog
        title: "Import Media"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Media (*.mp4 *.mov *.mkv *.mp3 *.wav *.aac *.png *.jpg *.jpeg)", "All files (*)"]
        onAccepted: {
            for (var i = 0; i < selectedFiles.length; ++i) session.importMedia(selectedFiles[i])
        }
    }

    FileDialog {
        id: srtImportDialog
        title: "Import Subtitles"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Subtitles (*.srt *.vtt)", "All files (*)"]
        onAccepted: session.importSubtitlesFile(selectedFile)
    }

    FileDialog {
        id: srtExportDialog
        title: "Export Subtitles"
        fileMode: FileDialog.SaveFile
        nameFilters: ["SubRip Subtitles (*.srt)"]
        onAccepted: session.exportSubtitlesFile(selectedFile)
    }
}
