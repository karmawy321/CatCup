import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Right column: selection-driven properties. Every control shows explicit
// units; every edit goes through an undoable command. Numeric edits apply on
// Enter (editingFinished) — no live-drag spam on the undo stack.

Pane {
    id: root
    padding: 8
    background: Rectangle { color: "#1e1e1e" }

    property var info: selection.selectedClipId !== ""
        ? timeline.clipInfo(selection.selectedClipId) : null

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        Label { text: "Inspector"; font.bold: true }

        Label {
            visible: root.info === null || root.info.clipId === undefined
            text: "Nothing selected.\nClick a timeline clip."
            opacity: 0.6
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            visible: root.info !== null && root.info.clipId !== undefined
            spacing: 6
            Layout.fillWidth: true

            Label { text: (root.info ? root.info.name : "") + "  ·  " + (root.info ? root.info.kind : ""); font.bold: true }

            GridLayout {
                columns: 2
                columnSpacing: 8
                rowSpacing: 4
                Layout.fillWidth: true

                Label { text: "Start (s)" }
                DoubleField {
                    from: 0; to: 3600
                    value: root.info ? root.info.startSec : 0
                    onApply: (v) => session.moveClipTo(selection.selectedClipId, v)
                }
                Label { text: "Duration (s)" }
                DoubleField {
                    from: 0.1; to: 3600
                    value: root.info ? root.info.durationSec : 0
                    onApply: (v) => {
                        var i = root.info
                        session.trimClip(selection.selectedClipId, i.sourceInSec, i.sourceInSec + v, i.startSec)
                    }
                }
            }

            GroupBox {
                title: "Transform"
                Layout.fillWidth: true
                GridLayout {
                    columns: 2
                    columnSpacing: 8
                    rowSpacing: 4
                    anchors.fill: parent
                    Label { text: "Scale (×)" }
                    DoubleField {
                        from: 0.1; to: 8
                        value: root.info ? root.info.scale : 1
                        onApply: (v) => applyTransform({scale: v})
                    }
                    Label { text: "X (px)" }
                    DoubleField {
                        from: -2000; to: 2000
                        value: root.info ? root.info.x : 0
                        onApply: (v) => applyTransform({x: v})
                    }
                    Label { text: "Y (px)" }
                    DoubleField {
                        from: -2000; to: 2000
                        value: root.info ? root.info.y : 0
                        onApply: (v) => applyTransform({y: v})
                    }
                    Label { text: "Rotation (°)" }
                    DoubleField {
                        from: -180; to: 180
                        value: root.info ? root.info.rotation : 0
                        onApply: (v) => applyTransform({rotation: v})
                    }
                }
            }

            GroupBox {
                title: "Title"
                visible: root.info && (root.info.kind === "text" || root.info.text !== "")
                Layout.fillWidth: true
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 4
                    TextField {
                        id: titleText
                        Layout.fillWidth: true
                        text: root.info ? root.info.text : ""
                    }
                    RowLayout {
                        TextField {
                            id: titleFont
                            Layout.fillWidth: true
                            text: root.info && root.info.fontFamily !== "" ? root.info.fontFamily : "Arial"
                        }
                        SpinBox {
                            id: titleSize
                            from: 8
                            to: 400
                            value: root.info ? Math.round(root.info.fontSizePt) : 72
                        }
                    }
                    Button {
                        text: "Apply text"
                        onClicked: session.setClipText(selection.selectedClipId, titleText.text, titleFont.text, titleSize.value)
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }
    }

    function applyTransform(patch) {
        var i = root.info
        session.setClipTransform(
            selection.selectedClipId,
            patch.scale !== undefined ? patch.scale : i.scale,
            patch.x !== undefined ? patch.x : i.x,
            patch.y !== undefined ? patch.y : i.y,
            patch.rotation !== undefined ? patch.rotation : i.rotation)
    }

    // Numeric field with slider: coarse drag, exact type-in. Applies once per
    // gesture (slider release / Enter) so each edit is one undo step.
    component DoubleField : RowLayout {
        property double value: 0
        property double from: -500
        property double to: 500
        signal apply(double v)
        spacing: 4
        Layout.fillWidth: true
        Slider {
            Layout.fillWidth: true
            from: parent.from
            to: parent.to
            stepSize: (parent.to - parent.from) / 1000
            value: parent.value
            onPressedChanged: if (!pressed) parent.apply(value)
        }
        TextField {
            Layout.preferredWidth: 76
            text: Number(parent.value).toFixed(2)
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            onEditingFinished: parent.apply(Number(text))
        }
    }
}
