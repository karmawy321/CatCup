import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// Left column: 2026 Obsidian Asset Browser with Media, Text, Transitions,
// Effects, and Smart AI Tools.

Rectangle {
    id: root
    color: Theme.bgSidebar
    border.color: Theme.borderSubtle
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // Modern Segmented Tab Bar
        Rectangle {
            Layout.fillWidth: true
            height: 36
            radius: Theme.radiusMedium
            color: Theme.bgApp
            border.color: Theme.borderSubtle
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.margins: 3
                spacing: 2

                Repeater {
                    model: ["Media", "Text", "Trans", "FX", "Smart"]
                    delegate: Rectangle {
                        id: tabDelegate
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 4
                        color: tabsStack.currentIndex === index ? Theme.bgActive : (tabMouse.containsMouse ? Theme.bgElevated : "transparent")
                        border.color: tabsStack.currentIndex === index ? Theme.borderFocus : "transparent"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            font.family: Theme.fontBody
                            font.pixelSize: 11
                            font.weight: tabsStack.currentIndex === index ? Font.Bold : Font.Normal
                            color: tabsStack.currentIndex === index ? Theme.accent : (tabMouse.containsMouse ? Theme.textPrimary : Theme.textSecondary)
                        }

                        MouseArea {
                            id: tabMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: tabsStack.currentIndex = index
                        }
                    }
                }
            }
        }

        // Modern Search Input
        Rectangle {
            Layout.fillWidth: true
            height: 32
            radius: Theme.radiusSmall
            color: Theme.bgApp
            border.color: searchInput.activeFocus ? Theme.borderFocus : Theme.borderMedium
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                Text {
                    text: "🔍"
                    font.pixelSize: 11
                    color: Theme.textTertiary
                }

                TextInput {
                    id: searchInput
                    Layout.fillWidth: true
                    font.family: Theme.fontBody
                    font.pixelSize: 11
                    color: Theme.textPrimary
                    selectByMouse: true

                    Text {
                        anchors.fill: parent
                        text: "Search assets…"
                        font.family: Theme.fontBody
                        font.pixelSize: 11
                        color: Theme.textTertiary
                        visible: parent.text === "" && !parent.activeFocus
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Text {
                    visible: searchInput.text !== ""
                    text: "✕"
                    font.pixelSize: 10
                    color: Theme.textTertiary
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: searchInput.text = ""
                    }
                }
            }
        }

        // Tab Pages
        StackLayout {
            id: tabsStack
            currentIndex: 0
            Layout.fillWidth: true
            Layout.fillHeight: true

            // ==================== TAB 0: MEDIA ====================
            ColumnLayout {
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    StudioButton {
                        text: "Import Media"
                        iconText: "+"
                        variant: "accent"
                        Layout.fillWidth: true
                        onClicked: importDialog.open()
                    }

                    StudioButton {
                        text: "Add"
                        iconText: "↓"
                        variant: "secondary"
                        enabled: mediaList.currentIndex >= 0
                        onClicked: addCurrent()
                    }
                }

                ListView {
                    id: mediaList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: library

                    delegate: Rectangle {
                        id: mediaCard
                        width: mediaList.width
                        visible: assetName.toLowerCase().indexOf(searchInput.text.toLowerCase()) >= 0
                        height: visible ? 68 : 0
                        radius: Theme.radiusMedium
                        color: ListView.isCurrentItem ? Theme.bgActive : (cardMouse.containsMouse ? Theme.bgHover : Theme.bgCard)
                        border.color: ListView.isCurrentItem ? Theme.borderFocus : Theme.borderMedium
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 8

                            // Thumbnail with duration chip
                            Rectangle {
                                Layout.preferredWidth: 96
                                Layout.preferredHeight: 54
                                radius: Theme.radiusSmall
                                color: "#000000"
                                clip: true

                                Image {
                                    anchors.fill: parent
                                    fillMode: Image.PreserveAspectCrop
                                    asynchronous: true
                                    cache: true
                                    source: "image://thumb/" + assetId
                                }

                                // Duration pill overlay
                                Rectangle {
                                    anchors.right: parent.right
                                    anchors.bottom: parent.bottom
                                    anchors.margins: 3
                                    radius: 3
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
                            }

                            // Asset Details
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: assetName
                                    font.family: Theme.fontBody
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    color: Theme.textPrimary
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }

                                Row {
                                    spacing: 4
                                    Rectangle {
                                        radius: 2
                                        color: assetKind === "video" ? "#163836" : (assetKind === "audio" ? "#183820" : "#2E1C44")
                                        implicitWidth: kindText.implicitWidth + 6
                                        implicitHeight: kindText.implicitHeight + 2

                                        Text {
                                            id: kindText
                                            anchors.centerIn: parent
                                            text: assetKind.toUpperCase()
                                            font.family: Theme.fontMono
                                            font.pixelSize: 8
                                            font.bold: true
                                            color: assetKind === "video" ? Theme.cyan : (assetKind === "audio" ? Theme.accent : Theme.purple)
                                        }
                                    }
                                }
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
                }
            }

            // ==================== TAB 1: TEXT ====================
            ScrollView {
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: 10

                    StudioCard {
                        title: "Title & Lower Thirds"
                        iconText: "🔤"
                        Layout.fillWidth: true

                        Text {
                            text: "Title Text"
                            font.family: Theme.fontBody
                            font.pixelSize: 11
                            color: Theme.textSecondary
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 32
                            radius: Theme.radiusSmall
                            color: Theme.bgElevated
                            border.color: titleField.activeFocus ? Theme.borderFocus : Theme.borderMedium
                            border.width: 1

                            TextInput {
                                id: titleField
                                anchors.fill: parent
                                anchors.margins: 6
                                font.family: Theme.fontBody
                                font.pixelSize: 12
                                color: Theme.textPrimary
                                selectByMouse: true
                                text: "CatCup Cinematic Title"
                            }
                        }

                        StudioButton {
                            text: "Insert Title Clip (3s)"
                            iconText: "✨"
                            variant: "primary"
                            Layout.fillWidth: true
                            onClicked: {
                                var id = session.addTitle(titleField.text)
                                if (id !== "")
                                    selection.select(id)
                            }
                        }

                        Text {
                            text: "Titles land on T1 at the timeline end. Select the title on the timeline to customize font, scale, and color in the Inspector."
                            wrapMode: Text.WordWrap
                            font.family: Theme.fontBody
                            font.pixelSize: 10
                            color: Theme.textTertiary
                            Layout.fillWidth: true
                        }
                    }
                }
            }

            // ==================== TAB 2: TRANSITIONS ====================
            ColumnLayout {
                spacing: 8

                ListView {
                    id: transitionList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: ListModel {
                        ListElement { name: "Crossfade"; typeName: "crossfade"; desc: "Smooth linear cross-dissolve"; iconSymbol: "⧖" }
                        ListElement { name: "Dip to Black"; typeName: "dip_black"; desc: "Fade through deep obsidian black"; iconSymbol: "◼" }
                        ListElement { name: "Dip to White"; typeName: "dip_white"; desc: "Flash through white burst"; iconSymbol: "◻" }
                        ListElement { name: "Wipe Left"; typeName: "wipe_left"; desc: "Horizontal wipe sliding left"; iconSymbol: "◀" }
                        ListElement { name: "Wipe Right"; typeName: "wipe_right"; desc: "Horizontal wipe sliding right"; iconSymbol: "▶" }
                        ListElement { name: "Wipe Up"; typeName: "wipe_up"; desc: "Vertical wipe sliding up"; iconSymbol: "▲" }
                        ListElement { name: "Wipe Down"; typeName: "wipe_down"; desc: "Vertical wipe sliding down"; iconSymbol: "▼" }
                    }

                    delegate: Rectangle {
                        width: transitionList.width
                        visible: name.toLowerCase().indexOf(searchInput.text.toLowerCase()) >= 0
                        height: visible ? 54 : 0
                        radius: Theme.radiusMedium
                        color: ListView.isCurrentItem ? Theme.bgActive : (transMouse.containsMouse ? Theme.bgHover : Theme.bgCard)
                        border.color: ListView.isCurrentItem ? Theme.borderFocus : Theme.borderMedium
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 8

                            Rectangle {
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                radius: Theme.radiusSmall
                                color: Theme.bgElevated

                                Text {
                                    anchors.centerIn: parent
                                    text: iconSymbol
                                    font.pixelSize: 14
                                    color: Theme.orange
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: name
                                    font.family: Theme.fontBody
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    color: Theme.textPrimary
                                }
                                Text {
                                    text: desc
                                    font.family: Theme.fontBody
                                    font.pixelSize: 9
                                    color: Theme.textTertiary
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        MouseArea {
                            id: transMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: transitionList.currentIndex = index
                            onDoubleClicked: applyCurrentTransition()
                        }
                    }
                }

                StudioButton {
                    text: "Apply Transition (1.0s)"
                    iconText: "⚡"
                    variant: "accent"
                    Layout.fillWidth: true
                    enabled: transitionList.currentIndex >= 0 && selection.selectedClipId !== ""
                    onClicked: applyCurrentTransition()
                }
            }

            // ==================== TAB 3: EFFECTS ====================
            ColumnLayout {
                spacing: 8

                ListView {
                    id: effectList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: ListModel {
                        ListElement { name: "Vignette"; typeName: "vignette"; desc: "Cinematic dark edge shading"; isPreset: false; iconSymbol: "◎" }
                        ListElement { name: "Box Blur"; typeName: "blur"; desc: "Soft focal blur effect"; isPreset: false; iconSymbol: "🌫" }
                        ListElement { name: "Sharpen"; typeName: "sharpen"; desc: "Crisp edge definition enhancer"; isPreset: false; iconSymbol: "✦" }
                        ListElement { name: "Chroma Key"; typeName: "chroma_key"; desc: "Green screen background removal"; isPreset: false; iconSymbol: "🟩" }
                        ListElement { name: "Warm Cinema"; typeName: "preset_warm"; desc: "Golden hour warm tones (+temp, +contrast)"; isPreset: true; iconSymbol: "🌅" }
                        ListElement { name: "Cool Film"; typeName: "preset_cool"; desc: "Modern teal film look (-temp, +contrast)"; isPreset: true; iconSymbol: "❄" }
                        ListElement { name: "Black & White"; typeName: "preset_bw"; desc: "Classic monochrome (0 saturation)"; isPreset: true; iconSymbol: "◑" }
                        ListElement { name: "Vibrant Punch"; typeName: "preset_vibrant"; desc: "Rich color pop (+saturation, +contrast)"; isPreset: true; iconSymbol: "🌈" }
                    }

                    delegate: Rectangle {
                        width: effectList.width
                        visible: name.toLowerCase().indexOf(searchInput.text.toLowerCase()) >= 0
                        height: visible ? 54 : 0
                        radius: Theme.radiusMedium
                        color: ListView.isCurrentItem ? Theme.bgActive : (fxMouse.containsMouse ? Theme.bgHover : Theme.bgCard)
                        border.color: ListView.isCurrentItem ? Theme.borderFocus : Theme.borderMedium
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 8

                            Rectangle {
                                Layout.preferredWidth: 36
                                Layout.preferredHeight: 36
                                radius: Theme.radiusSmall
                                color: Theme.bgElevated

                                Text {
                                    anchors.centerIn: parent
                                    text: iconSymbol
                                    font.pixelSize: 14
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    text: name
                                    font.family: Theme.fontBody
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    color: Theme.textPrimary
                                }
                                Text {
                                    text: desc
                                    font.family: Theme.fontBody
                                    font.pixelSize: 9
                                    color: Theme.textTertiary
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }

                        MouseArea {
                            id: fxMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: effectList.currentIndex = index
                            onDoubleClicked: applyCurrentEffect()
                        }
                    }
                }

                StudioButton {
                    text: "Apply Effect to Clip"
                    iconText: "✨"
                    variant: "accent"
                    Layout.fillWidth: true
                    enabled: effectList.currentIndex >= 0 && selection.selectedClipId !== ""
                    onClicked: applyCurrentEffect()
                }
            }

            // ==================== TAB 4: SMART TOOLS ====================
            ScrollView {
                contentWidth: availableWidth
                clip: true

                ColumnLayout {
                    width: parent.width
                    spacing: 10

                    // Card 1: AI Auto-Captions
                    StudioCard {
                        title: "AI Auto-Captions"
                        iconText: "💬"
                        collapsible: true
                        Layout.fillWidth: true

                        Text {
                            text: "Paste transcript / speech text:"
                            font.family: Theme.fontBody
                            font.pixelSize: 10
                            color: Theme.textSecondary
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 64
                            radius: Theme.radiusSmall
                            color: Theme.bgElevated
                            border.color: transcriptArea.activeFocus ? Theme.borderFocus : Theme.borderMedium
                            border.width: 1

                            ScrollView {
                                anchors.fill: parent
                                anchors.margins: 4
                                clip: true

                                TextArea {
                                    id: transcriptArea
                                    font.family: Theme.fontBody
                                    font.pixelSize: 11
                                    color: Theme.textPrimary
                                    wrapMode: Text.WordWrap
                                    placeholderText: "Type or paste transcript here…"
                                }
                            }
                        }

                        StudioSlider {
                            id: wordsSlider
                            label: "Words per Cue"
                            from: 1
                            to: 10
                            stepSize: 1
                            value: 4
                            decimals: 0
                            unit: "words"
                            Layout.fillWidth: true
                        }

                        StudioButton {
                            text: "✨ Generate Captions"
                            variant: "primary"
                            Layout.fillWidth: true
                            onClicked: {
                                if (transcriptArea.text !== "") {
                                    session.generateAutoCaptions(transcriptArea.text, Math.round(wordsSlider.value))
                                }
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            StudioButton {
                                text: "Import SRT"
                                compact: true
                                variant: "secondary"
                                Layout.fillWidth: true
                                onClicked: srtImportDialog.open()
                            }

                            StudioButton {
                                text: "Export SRT"
                                compact: true
                                variant: "secondary"
                                Layout.fillWidth: true
                                onClicked: srtExportDialog.open()
                            }
                        }
                    }

                    // Card 2: Smart Silence Cut
                    StudioCard {
                        title: "Smart Silence Cut"
                        iconText: "✂"
                        collapsible: true
                        Layout.fillWidth: true

                        Text {
                            text: "Detects silence in audio/video and ripple-joins speech portions."
                            wrapMode: Text.WordWrap
                            font.family: Theme.fontBody
                            font.pixelSize: 10
                            color: Theme.textTertiary
                            Layout.fillWidth: true
                        }

                        StudioSlider {
                            id: silenceThreshSlider
                            label: "Silence Threshold"
                            from: -60
                            to: -15
                            stepSize: 1
                            value: -35
                            decimals: 0
                            unit: "dB"
                            Layout.fillWidth: true
                        }

                        StudioSlider {
                            id: silenceDurSlider
                            label: "Min Duration"
                            from: 0.1
                            to: 2.0
                            stepSize: 0.1
                            value: 0.4
                            decimals: 1
                            unit: "s"
                            Layout.fillWidth: true
                        }

                        StudioButton {
                            text: "Cut Silence (Ripple)"
                            iconText: "✂"
                            variant: "accent"
                            Layout.fillWidth: true
                            enabled: selection.selectedClipId !== ""
                            onClicked: {
                                if (selection.selectedClipId !== "") {
                                    session.autoSilenceCut(selection.selectedClipId, silenceThreshSlider.value, silenceDurSlider.value)
                                }
                            }
                        }
                    }

                    // Card 3: Scene Cut Splitter
                    StudioCard {
                        title: "Scene Detection Split"
                        iconText: "🎬"
                        collapsible: true
                        Layout.fillWidth: true

                        Text {
                            text: "Scans video frames for cuts and splits clip into scene blocks."
                            wrapMode: Text.WordWrap
                            font.family: Theme.fontBody
                            font.pixelSize: 10
                            color: Theme.textTertiary
                            Layout.fillWidth: true
                        }

                        StudioSlider {
                            id: sceneThreshSlider
                            label: "Cut Sensitivity"
                            from: 0.10
                            to: 0.60
                            stepSize: 0.05
                            value: 0.25
                            decimals: 2
                            unit: "diff"
                            Layout.fillWidth: true
                        }

                        StudioButton {
                            text: "Split at Scene Cuts"
                            iconText: "🎬"
                            variant: "accent"
                            Layout.fillWidth: true
                            enabled: selection.selectedClipId !== ""
                            onClicked: {
                                if (selection.selectedClipId !== "") {
                                    session.autoSceneSplit(selection.selectedClipId, sceneThreshSlider.value)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    function applyCurrentEffect() {
        if (effectList.currentIndex < 0 || selection.selectedClipId === "")
            return
        var item = effectList.model.get(effectList.currentIndex)
        if (item.isPreset) {
            if (item.typeName === "preset_warm") {
                session.setClipColorAdjust(selection.selectedClipId, 0.05, 1.15, 1.1, 0.3, 0.05)
            } else if (item.typeName === "preset_cool") {
                session.setClipColorAdjust(selection.selectedClipId, 0.0, 1.15, 0.95, -0.3, -0.05)
            } else if (item.typeName === "preset_bw") {
                session.setClipColorAdjust(selection.selectedClipId, 0.0, 1.2, 0.0, 0.0, 0.0)
            } else if (item.typeName === "preset_vibrant") {
                session.setClipColorAdjust(selection.selectedClipId, 0.05, 1.2, 1.45, 0.05, 0.0)
            }
        } else {
            session.addClipEffect(selection.selectedClipId, item.typeName)
        }
    }

    function addCurrent() {
        if (mediaList.currentIndex < 0)
            return
        var id = session.addClipToTimeline(library.assetIdAt(mediaList.currentIndex))
        if (id !== "")
            selection.select(id)
    }

    function applyCurrentTransition() {
        if (transitionList.currentIndex < 0 || selection.selectedClipId === "")
            return
        var item = transitionList.model.get(transitionList.currentIndex)
        var clipInfo = timeline.clipInfo(selection.selectedClipId)
        if (!clipInfo || !clipInfo.trackId)
            return

        var nextClip = timeline.adjacentClipId(selection.selectedClipId, true)
        var prevClip = timeline.adjacentClipId(selection.selectedClipId, false)
        var fromId = selection.selectedClipId
        var toId = nextClip
        if (toId === "" && prevClip !== "") {
            fromId = prevClip
            toId = selection.selectedClipId
        }

        var transId = session.addTransition(clipInfo.trackId, fromId, toId, item.typeName, 1.0, 0)
        if (transId !== "") {
            selection.selectTransition(transId)
        }
    }

    FileDialog {
        id: importDialog
        title: "Import Media"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Media (*.mp4 *.mov *.mkv *.mp3 *.wav *.aac *.png *.jpg *.jpeg)", "All files (*)"]
        onAccepted: {
            for (var i = 0; i < selectedFiles.length; ++i)
                session.importMedia(selectedFiles[i])
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
