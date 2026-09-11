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
    property var transInfo: selection.selectedTransitionId !== ""
        ? timeline.transitionInfo(selection.selectedTransitionId) : null

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: root.availableWidth
            spacing: 6

            Label { text: "Inspector"; font.bold: true }

            Label {
                visible: (root.info === null || root.info.clipId === undefined) &&
                         (root.transInfo === null || root.transInfo.id === undefined)
                text: "Nothing selected.\nClick a timeline clip or transition."
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
                    Label { text: "Opacity" }
                    DoubleField {
                        from: 0.0; to: 1.0
                        value: root.info && root.info.opacity !== undefined ? root.info.opacity : 1.0
                        onApply: (v) => session.setClipOpacity(selection.selectedClipId, Math.max(0.0, Math.min(1.0, v)))
                    }
                }

                // Speed controls
                GroupBox {
                    title: "Speed"
                    Layout.fillWidth: true
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 4
                        DoubleField {
                            from: 0.1; to: 10.0
                            value: root.info && root.info.speed !== undefined ? root.info.speed : 1.0
                            onApply: (v) => session.setClipSpeed(selection.selectedClipId, Math.max(0.1, Math.min(10.0, v)))
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Button {
                                text: "0.5×"
                                Layout.fillWidth: true
                                onClicked: session.setClipSpeed(selection.selectedClipId, 0.5)
                            }
                            Button {
                                text: "1.0×"
                                Layout.fillWidth: true
                                onClicked: session.setClipSpeed(selection.selectedClipId, 1.0)
                            }
                            Button {
                                text: "2.0×"
                                Layout.fillWidth: true
                                onClicked: session.setClipSpeed(selection.selectedClipId, 2.0)
                            }
                            Button {
                                text: "4.0×"
                                Layout.fillWidth: true
                                onClicked: session.setClipSpeed(selection.selectedClipId, 4.0)
                            }
                        }
                    }
                }

                // Color Adjustment
                GroupBox {
                    title: "Color Adjustment"
                    Layout.fillWidth: true
                    GridLayout {
                        columns: 2
                        columnSpacing: 8
                        rowSpacing: 4
                        anchors.fill: parent

                        Label { text: "Brightness" }
                        DoubleField {
                            from: -1.0; to: 1.0
                            value: root.info && root.info.brightness !== undefined ? root.info.brightness : 0.0
                            onApply: (v) => applyColor(v, root.info.contrast, root.info.saturation, root.info.temperature, root.info.tint)
                        }
                        Label { text: "Contrast" }
                        DoubleField {
                            from: 0.0; to: 2.0
                            value: root.info && root.info.contrast !== undefined ? root.info.contrast : 1.0
                            onApply: (v) => applyColor(root.info.brightness, v, root.info.saturation, root.info.temperature, root.info.tint)
                        }
                        Label { text: "Saturation" }
                        DoubleField {
                            from: 0.0; to: 2.0
                            value: root.info && root.info.saturation !== undefined ? root.info.saturation : 1.0
                            onApply: (v) => applyColor(root.info.brightness, root.info.contrast, v, root.info.temperature, root.info.tint)
                        }
                        Label { text: "Temperature" }
                        DoubleField {
                            from: -1.0; to: 1.0
                            value: root.info && root.info.temperature !== undefined ? root.info.temperature : 0.0
                            onApply: (v) => applyColor(root.info.brightness, root.info.contrast, root.info.saturation, v, root.info.tint)
                        }
                        Label { text: "Tint" }
                        DoubleField {
                            from: -1.0; to: 1.0
                            value: root.info && root.info.tint !== undefined ? root.info.tint : 0.0
                            onApply: (v) => applyColor(root.info.brightness, root.info.contrast, root.info.saturation, root.info.temperature, v)
                        }
                        Button {
                            text: "Reset Color"
                            Layout.columnSpan: 2
                            Layout.fillWidth: true
                            onClicked: applyColor(0.0, 1.0, 1.0, 0.0, 0.0)
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

                // Applied Effects Stack
                GroupBox {
                    title: "Effects Stack"
                    Layout.fillWidth: true
                    visible: root.info && root.info.effects && root.info.effects.length > 0
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6
                        Repeater {
                            model: root.info ? root.info.effects : []
                            delegate: Rectangle {
                                id: outerDelegate
                                property int effectIndex: index
                                property string effectType: modelData.type
                                Layout.fillWidth: true
                                implicitHeight: effCol.implicitHeight + 8
                                color: "#252525"
                                radius: 4
                                border.color: "#383838"
                                ColumnLayout {
                                    id: effCol
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 4
                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            text: outerDelegate.effectType.toUpperCase()
                                            font.bold: true
                                            Layout.fillWidth: true
                                        }
                                        Button {
                                            text: "✕"
                                            implicitWidth: 28
                                            implicitHeight: 24
                                            onClicked: session.removeClipEffect(selection.selectedClipId, outerDelegate.effectIndex)
                                        }
                                    }
                                    // Numerical params
                                    Repeater {
                                        model: {
                                            if (!modelData.params || modelData.type === "color_adjust") return []
                                            var keys = Object.keys(modelData.params)
                                            var arr = []
                                            for (var i = 0; i < keys.length; ++i) {
                                                var k = keys[i]
                                                arr.push({ name: k, val: modelData.params[k] })
                                            }
                                            return arr
                                        }
                                        delegate: RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 4
                                            Label {
                                                text: modelData.name
                                                Layout.preferredWidth: 70
                                                elide: Text.ElideRight
                                            }
                                            DoubleField {
                                                from: (modelData.name === "intensity" || modelData.name === "smoothness" || modelData.name === "similarity") ? 0.0 : (modelData.name === "radius" && outerDelegate.effectType === "blur" ? 1.0 : 0.0)
                                                to: (modelData.name === "radius" && outerDelegate.effectType === "blur") ? 50.0 : (modelData.name === "amount" ? 5.0 : 2.0)
                                                value: modelData.val
                                                onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, outerDelegate.effectIndex, modelData.name, v)
                                            }
                                        }
                                    }
                                    // String params (e.g. key_color)
                                    Repeater {
                                        model: {
                                            if (!modelData.strParams) return []
                                            var keys = Object.keys(modelData.strParams)
                                            var arr = []
                                            for (var i = 0; i < keys.length; ++i) {
                                                var k = keys[i]
                                                arr.push({ name: k, val: modelData.strParams[k] })
                                            }
                                            return arr
                                        }
                                        delegate: RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 4
                                            Label { text: modelData.name; Layout.preferredWidth: 70 }
                                            TextField {
                                                Layout.fillWidth: true
                                                text: modelData.val
                                                onEditingFinished: session.updateClipEffectStrParam(selection.selectedClipId, outerDelegate.effectIndex, modelData.name, text)
                                            }
                                        }
                                    }
                                }
                            }
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

            // ---- Transition Inspector
            ColumnLayout {
                visible: root.transInfo !== null && root.transInfo.id !== undefined
                spacing: 6
                Layout.fillWidth: true

                Label { text: "Transition: " + (root.transInfo ? root.transInfo.type : ""); font.bold: true }

                GridLayout {
                    columns: 2
                    columnSpacing: 8
                    rowSpacing: 6
                    Layout.fillWidth: true

                    Label { text: "Type" }
                    ComboBox {
                        id: typeCombo
                        Layout.fillWidth: true
                        model: ["crossfade", "dip_black", "dip_white", "wipe_left", "wipe_right", "wipe_up", "wipe_down"]
                        currentIndex: {
                            if (!root.transInfo) return 0
                            var idx = model.indexOf(root.transInfo.type)
                            return idx >= 0 ? idx : 0
                        }
                        onActivated: {
                            if (root.transInfo && selection.selectedTransitionId !== "") {
                                session.updateTransition(
                                    selection.selectedTransitionId,
                                    root.transInfo.durationSec,
                                    alignCombo.currentIndex,
                                    currentText,
                                    "linear"
                                )
                            }
                        }
                    }

                    Label { text: "Duration (s)" }
                    DoubleField {
                        id: transDurField
                        from: 0.1; to: 10.0
                        value: root.transInfo ? root.transInfo.durationSec : 1.0
                        onApply: (v) => {
                            if (root.transInfo && selection.selectedTransitionId !== "") {
                                session.updateTransition(
                                    selection.selectedTransitionId,
                                    v,
                                    alignCombo.currentIndex,
                                    typeCombo.currentText,
                                    "linear"
                                )
                            }
                        }
                    }

                    Label { text: "Alignment" }
                    ComboBox {
                        id: alignCombo
                        Layout.fillWidth: true
                        model: ["Center on Cut", "Start on Cut", "End on Cut"]
                        currentIndex: {
                            if (!root.transInfo) return 0
                            var al = root.transInfo.alignment
                            if (al === "start") return 1
                            if (al === "end") return 2
                            return 0
                        }
                        onActivated: {
                            if (root.transInfo && selection.selectedTransitionId !== "") {
                                session.updateTransition(
                                    selection.selectedTransitionId,
                                    root.transInfo.durationSec,
                                    currentIndex,
                                    typeCombo.currentText,
                                    "linear"
                                )
                            }
                        }
                    }
                }

                Button {
                    text: "Delete Transition"
                    highlighted: true
                    Layout.fillWidth: true
                    onClicked: {
                        if (selection.selectedTransitionId !== "") {
                            session.removeTransition(selection.selectedTransitionId)
                            selection.clearSelection()
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
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

    function applyColor(b, c, s, temp, tint) {
        session.setClipColorAdjust(selection.selectedClipId, b, c, s, temp, tint)
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
