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
        }
    }

    function addCurrent() {
        if (mediaList.currentIndex < 0)
            return
        var id = session.addClipToTimeline(library.assetIdAt(mediaList.currentIndex))
        if (id !== "")
            selection.select(id)
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
