import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

// Left column: asset tabs (Media / Text), searchable cards with thumbnails,
// import, and add-to-timeline. A browser card preview never edits — only the
// explicit add action inserts (per the observed interaction model).

Pane {
    id: root
    padding: 8
    background: Rectangle { color: "#1e1e1e" }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        TabBar {
            id: tabs
            Layout.fillWidth: true
            TabButton { text: "Media" }
            TabButton { text: "Text" }
            TabButton { text: "Transitions" }
            TabButton { text: "Effects" }
        }

        TextField {
            id: search
            Layout.fillWidth: true
            placeholderText: "Search assets…"
        }

        StackLayout {
            currentIndex: tabs.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            // ---- Media tab
            ColumnLayout {
                RowLayout {
                    Button {
                        text: "Import…"
                        highlighted: true
                        onClicked: importDialog.open()
                    }
                    Button {
                        text: "Add to timeline"
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
                    highlight: Rectangle { color: "#2dd4bf"; opacity: 0.25; radius: 4 }
                    delegate: ItemDelegate {
                        width: mediaList.width
                        highlighted: ListView.isCurrentItem
                        visible: assetName.toLowerCase().indexOf(search.text.toLowerCase()) >= 0
                        height: visible ? 64 : 0
                        onClicked: mediaList.currentIndex = index
                        onDoubleClicked: addCurrent()
                        contentItem: RowLayout {
                            spacing: 8
                            Image {
                                Layout.preferredWidth: 96
                                Layout.preferredHeight: 54
                                fillMode: Image.PreserveAspectFit
                                asynchronous: true
                                cache: true
                                source: "image://thumb/" + assetId
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Label { text: assetName; elide: Text.ElideRight; Layout.fillWidth: true }
                                Label {
                                    text: assetKind + " · " + durationSec.toFixed(1) + " s"
                                    opacity: 0.6
                                    font.pointSize: 8
                                }
                            }
                        }
                    }
                }
            }

            // ---- Text tab
            ColumnLayout {
                Label { text: "Title text"; opacity: 0.7 }
                TextField {
                    id: titleField
                    Layout.fillWidth: true
                    text: "Hello edit"
                }
                Button {
                    text: "Add title (3 s)"
                    highlighted: true
                    onClicked: {
                        var id = session.addTitle(titleField.text)
                        if (id !== "")
                            selection.select(id)
                    }
                }
                Label {
                    text: "Titles land on T1 at the track end. Select the title to edit text, font, and transform on the right."
                    wrapMode: Text.WordWrap
                    opacity: 0.6
                    Layout.fillWidth: true
                }
                Item { Layout.fillHeight: true }
            }

            // ---- Transitions tab
            ColumnLayout {
                spacing: 8
                Label { text: "Select a transition to apply between clips or to selected clip."; opacity: 0.7; font.pointSize: 8; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                ListView {
                    id: transitionList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: ListModel {
                        ListElement { name: "Crossfade"; typeName: "crossfade"; desc: "Smooth linear cross-dissolve" }
                        ListElement { name: "Dip to Black"; typeName: "dip_black"; desc: "Fade through black color" }
                        ListElement { name: "Dip to White"; typeName: "dip_white"; desc: "Fade through white flash" }
                        ListElement { name: "Wipe Left"; typeName: "wipe_left"; desc: "Horizontal wipe to the left" }
                        ListElement { name: "Wipe Right"; typeName: "wipe_right"; desc: "Horizontal wipe to the right" }
                        ListElement { name: "Wipe Up"; typeName: "wipe_up"; desc: "Vertical wipe sliding up" }
                        ListElement { name: "Wipe Down"; typeName: "wipe_down"; desc: "Vertical wipe sliding down" }
                    }
                    highlight: Rectangle { color: "#2dd4bf"; opacity: 0.25; radius: 4 }
                    delegate: ItemDelegate {
                        width: transitionList.width
                        highlighted: ListView.isCurrentItem
                        visible: name.toLowerCase().indexOf(search.text.toLowerCase()) >= 0
                        height: visible ? 54 : 0
                        onClicked: transitionList.currentIndex = index
                        onDoubleClicked: applyCurrentTransition()
                        contentItem: ColumnLayout {
                            spacing: 2
                            Label { text: name; font.bold: true }
                            Label { text: desc; opacity: 0.6; font.pointSize: 8 }
                        }
                    }
                }
                Button {
                    text: "Apply Transition (1.0 s)"
                    highlighted: true
                    Layout.fillWidth: true
                    enabled: transitionList.currentIndex >= 0 && selection.selectedClipId !== ""
                    onClicked: applyCurrentTransition()
                }
            }

            // ---- Effects tab
            ColumnLayout {
                spacing: 8
                Label { text: "Select an effect or color preset to apply to the selected clip."; opacity: 0.7; font.pointSize: 8; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                ListView {
                    id: effectList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 6
                    model: ListModel {
                        ListElement { name: "Vignette"; typeName: "vignette"; desc: "Cinematic dark edge shading"; isPreset: false }
                        ListElement { name: "Box Blur"; typeName: "blur"; desc: "Soft focal blur effect"; isPreset: false }
                        ListElement { name: "Sharpen"; typeName: "sharpen"; desc: "Edge definition enhancer"; isPreset: false }
                        ListElement { name: "Chroma Key"; typeName: "chroma_key"; desc: "Green screen background removal"; isPreset: false }
                        ListElement { name: "Warm Cinema"; typeName: "preset_warm"; desc: "Golden hour warm tones (+temp, +contrast)"; isPreset: true }
                        ListElement { name: "Cool Film"; typeName: "preset_cool"; desc: "Modern teal film look (-temp, +contrast)"; isPreset: true }
                        ListElement { name: "Black & White"; typeName: "preset_bw"; desc: "Classic monochrome (0 saturation)"; isPreset: true }
                        ListElement { name: "Vibrant Punch"; typeName: "preset_vibrant"; desc: "Rich color pop (+saturation, +contrast)"; isPreset: true }
                    }
                    highlight: Rectangle { color: "#2dd4bf"; opacity: 0.25; radius: 4 }
                    delegate: ItemDelegate {
                        width: effectList.width
                        highlighted: ListView.isCurrentItem
                        visible: name.toLowerCase().indexOf(search.text.toLowerCase()) >= 0
                        height: visible ? 54 : 0
                        onClicked: effectList.currentIndex = index
                        onDoubleClicked: applyCurrentEffect()
                        contentItem: ColumnLayout {
                            spacing: 2
                            Label { text: name; font.bold: true }
                            Label { text: desc; opacity: 0.6; font.pointSize: 8 }
                        }
                    }
                }
                Button {
                    text: "Apply Effect to Clip"
                    highlighted: true
                    Layout.fillWidth: true
                    enabled: effectList.currentIndex >= 0 && selection.selectedClipId !== ""
                    onClicked: applyCurrentEffect()
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
        title: "Import media"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Media (*.mp4 *.mov *.mkv *.mp3 *.wav *.aac *.png *.jpg *.jpeg)", "All files (*)"]
        onAccepted: {
            for (var i = 0; i < selectedFiles.length; ++i)
                session.importMedia(selectedFiles[i])
        }
    }
}
