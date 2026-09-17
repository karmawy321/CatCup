import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// Left column: CapCut Asset Browser with horizontal icon-tabs,
// sub-sidebar navigation, media cards with 'Added' chips, and AI tools.

Rectangle {
    id: root
    property var mainWindow: null
    property string targetRelinkAssetId: ""
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
                        Layout.leftMargin: 8
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
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 8
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

                        // CapCut Draft Interop Pill Button
                        Rectangle {
                            implicitWidth: 96
                            implicitHeight: 26
                            radius: 13
                            color: draftMouse.containsMouse ? Theme.bgHover : Theme.bgElevated
                            border.color: Theme.borderMedium
                            border.width: 1

                            Row {
                                anchors.centerIn: parent
                                spacing: 4
                                Text { text: "Draft"; font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textPrimary }
                                Text { text: "▾"; font.pixelSize: 9; color: Theme.textSecondary }
                            }

                            MouseArea {
                                id: draftMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: draftMenu.open()
                            }

                            Menu {
                                id: draftMenu
                                y: parent.height + 4
                                MenuItem {
                                    text: "Import CapCut Draft…"
                                    onTriggered: capcutDraftImportDialog.open()
                                }
                                MenuItem {
                                    text: "Export CapCut Draft…"
                                    onTriggered: capcutDraftExportDialog.open()
                                }
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

                    // Empty Media Library Placeholder
                    Rectangle {
                        visible: mediaList.count === 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 6
                        color: Theme.bgCard
                        border.color: Theme.borderMedium
                        border.width: 1

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 8
                            width: parent.width - 24

                            Text {
                                text: "📁"
                                font.pixelSize: 26
                                Layout.alignment: Qt.AlignHCenter
                            }

                            Text {
                                text: "No Media Imported"
                                font.family: Theme.fontBody
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                                Layout.alignment: Qt.AlignHCenter
                            }

                            Text {
                                text: "Click [+ Import] above to load video or audio files into the project"
                                font.family: Theme.fontBody
                                font.pixelSize: 10
                                color: Theme.textSecondary
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }

                    // Media Cards List (CapCut Style)
                    ListView {
                        id: mediaList
                        visible: mediaList.count > 0
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
                                border.color: isMissing ? Theme.danger : (ListView.isCurrentItem ? Theme.borderFocus : Theme.borderMedium)
                                border.width: isMissing ? 1.5 : 1
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

                                // Top-Right "Offline / Lost" Chip
                                Rectangle {
                                    visible: isMissing
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 4
                                    radius: 2
                                    color: "#D9EF4444"
                                    implicitWidth: lostBadge.implicitWidth + 8
                                    implicitHeight: lostBadge.implicitHeight + 2

                                    Text {
                                        id: lostBadge
                                        anchors.centerIn: parent
                                        text: "⚠ Lost"
                                        font.family: Theme.fontBody
                                        font.pixelSize: 8
                                        font.bold: true
                                        color: "#FFFFFF"
                                    }
                                }

                                // Center "Relink" Button when media is lost
                                Rectangle {
                                    visible: isMissing
                                    anchors.centerIn: parent
                                    radius: 4
                                    color: "#EE16161A"
                                    border.color: Theme.accent
                                    border.width: 1
                                    implicitWidth: relinkLabel.implicitWidth + 14
                                    implicitHeight: 22
                                    z: 5

                                    Text {
                                        id: relinkLabel
                                        anchors.centerIn: parent
                                        text: "Relink Media"
                                        font.family: Theme.fontBody
                                        font.pixelSize: 9
                                        font.bold: true
                                        color: Theme.accent
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            root.targetRelinkAssetId = assetId
                                            relinkDialog.open()
                                        }
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
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 8
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
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 8
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
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 8
                    spacing: 8

                    ListView {
                        id: fxList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 6
                        model: ListModel {
                            // Cinematic & Film Look
                            ListElement { name: "Letterbox 2.35:1"; typeName: "letterbox"; category: "Cinema"; desc: "CinemaScope 2.35:1 anamorphic black matte bars" }
                            ListElement { name: "35mm Film Grain"; typeName: "film_grain"; category: "Cinema"; desc: "Kodak Vision3 analog 35mm organic film grain" }
                            ListElement { name: "Cinematic Bloom"; typeName: "bloom"; category: "Cinema"; desc: "Black Pro-Mist specular highlight diffusion glow" }
                            ListElement { name: "Split Toning"; typeName: "split_toning"; category: "Cinema"; desc: "Hollywood teal shadows & golden amber highlights" }
                            ListElement { name: "Warm Cinema"; typeName: "preset_warm"; category: "Cinema"; desc: "Golden hour sunset movie tone" }
                            ListElement { name: "Cool Film"; typeName: "preset_cool"; category: "Cinema"; desc: "Modern cinematic cool film tone" }
                            ListElement { name: "Black & White"; typeName: "preset_bw"; category: "Cinema"; desc: "Classic high-contrast monochrome cinema look" }
                            ListElement { name: "Vibrant Punch"; typeName: "preset_vibrant"; category: "Cinema"; desc: "Rich saturated cinematic pop" }

                            // Optical & Lens
                            ListElement { name: "Chromatic Aberration"; typeName: "chromatic_aberration"; category: "Lens"; desc: "Prism optical lens dispersion & RGB edge split" }
                            ListElement { name: "Vignette"; typeName: "vignette"; category: "Lens"; desc: "Soft anamorphic edge shading" }
                            ListElement { name: "Box Blur"; typeName: "blur"; category: "Lens"; desc: "Smooth defocus blur filter" }
                            ListElement { name: "Sharpen"; typeName: "sharpen"; category: "Lens"; desc: "High-pass edge & detail enhancement" }
                            ListElement { name: "Chroma Key"; typeName: "chroma_key"; category: "Lens"; desc: "Green screen studio keying & spill suppression" }

                            // Stylize & Retro
                            ListElement { name: "Retro VHS Tape"; typeName: "retro_vhs"; category: "Retro"; desc: "80s CRT scanlines, color bleed & analog tape noise" }
                            ListElement { name: "Posterize"; typeName: "posterize"; category: "Stylize"; desc: "Graphic novel & pop-art color quantization" }
                            ListElement { name: "Invert Negative"; typeName: "invert"; category: "Stylize"; desc: "Film negative inversion & impact flash" }
                            ListElement { name: "Edge Detect"; typeName: "edge_detect"; category: "Stylize"; desc: "Sobel gradient sketch & neon outlines" }
                            ListElement { name: "Mirror Reflection"; typeName: "mirror"; category: "Stylize"; desc: "Horizontal, vertical, & 4-way kaleidoscope mirror" }
                        }

                        delegate: Rectangle {
                            width: fxList.width
                            height: 48
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
                                    spacing: 2
                                    RowLayout {
                                        spacing: 6
                                        Text { text: name; font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textPrimary }
                                        Rectangle {
                                            implicitWidth: catT.implicitWidth + 8
                                            implicitHeight: 14
                                            radius: 3
                                            color: Theme.bgElevated
                                            Text { id: catT; anchors.centerIn: parent; text: category; font.family: Theme.fontBody; font.pixelSize: 8; color: Theme.textTertiary }
                                        }
                                    }
                                    Text { text: desc; font.family: Theme.fontBody; font.pixelSize: 9; color: Theme.textTertiary; elide: Text.ElideRight; Layout.fillWidth: true }
                                }
                            }

                            MouseArea {
                                id: fxM
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    fxList.currentIndex = index
                                    applyFx()
                                }
                                onDoubleClicked: applyFx()
                            }
                        }
                    }

                    StudioButton {
                        text: "Apply Effect"
                        variant: "primary"
                        Layout.fillWidth: true
                        enabled: fxList.currentIndex >= 0
                        onClicked: applyFx()
                    }
                }

                // ==================== TAB 4: TRANSITIONS ====================
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 8
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
                            ListElement { name: "Iris Circle"; typeName: "iris_circle"; desc: "Classic Hollywood circular iris reveal" }
                            ListElement { name: "Barn Doors (H)"; typeName: "barn_doors_h"; desc: "Center stage curtains opening" }
                            ListElement { name: "Barn Doors (V)"; typeName: "barn_doors_v"; desc: "Vertical center split opening" }
                            ListElement { name: "Zoom In"; typeName: "zoom_in"; desc: "Dynamic cinematic punch zoom transition" }
                            ListElement { name: "Zoom Out"; typeName: "zoom_out"; desc: "Cinematic zoom pull transition" }
                            ListElement { name: "Flash Dissolve"; typeName: "flash_dissolve"; desc: "High-exposure warm gold flash dissolve" }
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
                                onClicked: {
                                    transList.currentIndex = index
                                    applyTrans()
                                }
                            }
                        }
                    }

                    StudioButton {
                        text: "Apply Transition"
                        variant: "primary"
                        Layout.fillWidth: true
                        enabled: transList.currentIndex >= 0
                        onClicked: applyTrans()
                    }
                }

                // ==================== TAB 5: CAPTIONS ====================
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.margins: 8
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
        if (fxList.currentIndex < 0) return
        var item = fxList.model.get(fxList.currentIndex)

        if (selection.selectedClipId === "") {
            footerBar.info("ℹ Select a clip on the timeline to apply " + item.name + " effect")
            return
        }

        if (item.typeName === "preset_warm") {
            session.setClipColorAdjust(selection.selectedClipId, 0.05, 1.15, 1.1, 0.3, 0.05)
        } else if (item.typeName === "preset_cool") {
            session.setClipColorAdjust(selection.selectedClipId, 0.0, 1.15, 0.95, -0.3, -0.05)
        } else if (item.typeName === "preset_bw") {
            session.setClipColorAdjust(selection.selectedClipId, 0.0, 1.2, 0.0, 0.0, 0.0)
        } else if (item.typeName === "preset_vibrant") {
            session.setClipColorAdjust(selection.selectedClipId, 0.05, 1.2, 1.45, 0.05, 0.0)
        } else if (typeof session.addClipEffect === 'function') {
            session.addClipEffect(selection.selectedClipId, item.typeName)
        }

        footerBar.info("✨ Applied " + item.name + " to selected clip")
    }

    function applyTrans() {
        if (transList.currentIndex < 0) return
        var item = transList.model.get(transList.currentIndex)

        if (selection.selectedClipId === "") {
            footerBar.info("ℹ Select a clip on the timeline to attach " + item.name + " transition")
            return
        }

        if (typeof session.addTransition !== 'function') {
            footerBar.info("ℹ Select adjacent clips on the timeline to add " + item.name + " transition")
            return
        }

        var clipInfo = timeline.clipInfo(selection.selectedClipId)
        if (!clipInfo || !clipInfo.trackId) {
            footerBar.error("Cannot add transition: could not determine track for clip")
            return
        }
        var nextClip = (typeof timeline.adjacentClipId === 'function') ? timeline.adjacentClipId(selection.selectedClipId, true) : ""
        var prevClip = (typeof timeline.adjacentClipId === 'function') ? timeline.adjacentClipId(selection.selectedClipId, false) : ""
        var fromId = selection.selectedClipId
        var toId = nextClip
        if (toId === "" && prevClip !== "") { fromId = prevClip; toId = selection.selectedClipId }
        if (fromId === "" || toId === "" || fromId === toId) {
            footerBar.error("Transitions require two adjacent clips on the same track.")
            return
        }
        var transId = session.addTransition(clipInfo.trackId, fromId, toId, item.typeName, 1.0, 0)
        if (transId !== "") {
            if (typeof selection.selectTransition === 'function') {
                selection.selectTransition(transId)
            }
            footerBar.info("⧖ Added " + item.name + " Transition")
        } else {
            footerBar.error("Failed to add transition between adjacent clips")
        }
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

    FileDialog {
        id: relinkDialog
        title: "Relink Media File"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Media Files (*.mp4 *.mov *.mkv *.mp3 *.wav *.aac *.png *.jpg *.jpeg)", "All files (*)"]
        onAccepted: {
            if (root.targetRelinkAssetId !== "") {
                var ok = library.relinkAsset(root.targetRelinkAssetId, selectedFile)
                if (ok) {
                    footerBar.info("Relinked media successfully")
                } else {
                    footerBar.error("Failed to relink media")
                }
                root.targetRelinkAssetId = ""
            }
        }
    }

    FileDialog {
        id: capcutDraftImportDialog
        title: "Import CapCut Draft"
        fileMode: FileDialog.OpenFile
        nameFilters: ["CapCut Draft (draft_content.json)", "JSON files (*.json)", "All files (*)"]
        onAccepted: {
            var ok = session.importCapCutDraft(selectedFile)
            if (ok) {
                footerBar.info("Imported CapCut Draft successfully")
            } else {
                footerBar.error("Failed to import CapCut Draft")
            }
        }
    }

    FileDialog {
        id: capcutDraftExportDialog
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
}
