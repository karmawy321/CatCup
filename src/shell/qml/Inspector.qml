import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Right column: 2026 Studio Property Inspector with collapsible obsidian
// cards, precision numeric sliders, and retiming presets.

Rectangle {
    id: root
    color: Theme.bgSidebar
    border.color: Theme.borderSubtle
    border.width: 1

    property var info: selection.selectedClipId !== ""
        ? timeline.clipInfo(selection.selectedClipId) : null
    property var transInfo: selection.selectedTransitionId !== ""
        ? timeline.transitionInfo(selection.selectedTransitionId) : null

    ScrollView {
        anchors.fill: parent
        anchors.margins: 10
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: root.availableWidth - 20
            spacing: 10

            // Inspector Header
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    text: "INSPECTOR"
                    font.family: Theme.fontMono
                    font.pixelSize: 11
                    font.bold: true
                    color: Theme.textSecondary
                    Layout.fillWidth: true
                }
            }

            // Empty State
            Rectangle {
                visible: (root.info === null || root.info.clipId === undefined) &&
                         (root.transInfo === null || root.transInfo.id === undefined)
                Layout.fillWidth: true
                height: 120
                radius: Theme.radiusMedium
                color: Theme.bgCard
                border.color: Theme.borderMedium
                border.width: 1

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 6

                    Text {
                        text: "🖱"
                        font.pixelSize: 20
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: "No clip or transition selected"
                        font.family: Theme.fontBody
                        font.pixelSize: 11
                        font.bold: true
                        color: Theme.textSecondary
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: "Click an item on the timeline to edit properties"
                        font.family: Theme.fontBody
                        font.pixelSize: 10
                        color: Theme.textTertiary
                        Layout.alignment: Qt.AlignHCenter
                    }
                }
            }

            // ==================== CLIP INSPECTOR ====================
            ColumnLayout {
                visible: root.info !== null && root.info.clipId !== undefined
                spacing: 10
                Layout.fillWidth: true

                // Clip Title Banner
                Rectangle {
                    Layout.fillWidth: true
                    height: 40
                    radius: Theme.radiusSmall
                    color: Theme.bgCard
                    border.color: Theme.borderMedium
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 6

                        Text {
                            text: (root.info ? root.info.name : "")
                            font.family: Theme.fontBody
                            font.pixelSize: 12
                            font.bold: true
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            implicitWidth: kindBadge.implicitWidth + 8
                            implicitHeight: 20
                            radius: 4
                            color: root.info && root.info.kind === "video" ? "#163836" : (root.info && root.info.kind === "audio" ? "#183820" : "#2E1C44")

                            Text {
                                id: kindBadge
                                anchors.centerIn: parent
                                text: root.info ? root.info.kind.toUpperCase() : ""
                                font.family: Theme.fontMono
                                font.pixelSize: 9
                                font.bold: true
                                color: root.info && root.info.kind === "video" ? Theme.cyan : (root.info && root.info.kind === "audio" ? Theme.accent : Theme.purple)
                            }
                        }
                    }
                }

                // 1. Timing & Placement Card
                StudioCard {
                    title: "Timing & Position"
                    iconText: "⏱"
                    collapsible: true
                    Layout.fillWidth: true

                    StudioSlider {
                        label: "Start Time"
                        from: 0
                        to: 3600
                        stepSize: 0.1
                        value: root.info ? root.info.startSec : 0
                        unit: "s"
                        Layout.fillWidth: true
                        onApply: (v) => session.moveClipTo(selection.selectedClipId, v)
                    }

                    StudioSlider {
                        label: "Duration"
                        from: 0.1
                        to: 3600
                        stepSize: 0.1
                        value: root.info ? root.info.durationSec : 0
                        unit: "s"
                        Layout.fillWidth: true
                        onApply: (v) => {
                            var i = root.info
                            session.trimClip(selection.selectedClipId, i.sourceInSec, i.sourceInSec + v, i.startSec)
                        }
                    }

                    StudioSlider {
                        label: "Opacity"
                        from: 0.0
                        to: 1.0
                        stepSize: 0.01
                        value: root.info && root.info.opacity !== undefined ? root.info.opacity : 1.0
                        decimals: 2
                        Layout.fillWidth: true
                        onApply: (v) => session.setClipOpacity(selection.selectedClipId, Math.max(0.0, Math.min(1.0, v)))
                    }
                }

                // 2. Speed & Retiming Card
                StudioCard {
                    title: "Speed & Retiming"
                    iconText: "⚡"
                    collapsible: true
                    Layout.fillWidth: true

                    StudioSlider {
                        id: speedSlider
                        label: "Playback Speed"
                        from: 0.1
                        to: 10.0
                        stepSize: 0.1
                        value: root.info && root.info.speed !== undefined ? root.info.speed : 1.0
                        unit: "×"
                        Layout.fillWidth: true
                        onApply: (v) => session.setClipSpeed(selection.selectedClipId, Math.max(0.1, Math.min(10.0, v)))
                    }

                    // Quick Retiming Preset Pills
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Repeater {
                            model: [0.5, 1.0, 2.0, 4.0]
                            delegate: StudioButton {
                                text: modelData.toFixed(1) + "×"
                                compact: true
                                variant: root.info && Math.abs(root.info.speed - modelData) < 0.05 ? "primary" : "secondary"
                                Layout.fillWidth: true
                                onClicked: session.setClipSpeed(selection.selectedClipId, modelData)
                            }
                        }
                    }
                }

                // 3. Color Grading Card
                StudioCard {
                    title: "Color Grading"
                    iconText: "🎨"
                    collapsible: true
                    Layout.fillWidth: true

                    StudioSlider {
                        label: "Brightness"
                        from: -1.0
                        to: 1.0
                        stepSize: 0.02
                        value: root.info && root.info.brightness !== undefined ? root.info.brightness : 0.0
                        Layout.fillWidth: true
                        onApply: (v) => applyColor(v, root.info.contrast, root.info.saturation, root.info.temperature, root.info.tint)
                    }

                    StudioSlider {
                        label: "Contrast"
                        from: 0.0
                        to: 2.0
                        stepSize: 0.02
                        value: root.info && root.info.contrast !== undefined ? root.info.contrast : 1.0
                        Layout.fillWidth: true
                        onApply: (v) => applyColor(root.info.brightness, v, root.info.saturation, root.info.temperature, root.info.tint)
                    }

                    StudioSlider {
                        label: "Saturation"
                        from: 0.0
                        to: 2.0
                        stepSize: 0.02
                        value: root.info && root.info.saturation !== undefined ? root.info.saturation : 1.0
                        Layout.fillWidth: true
                        onApply: (v) => applyColor(root.info.brightness, root.info.contrast, v, root.info.temperature, root.info.tint)
                    }

                    StudioSlider {
                        label: "Temperature"
                        from: -1.0
                        to: 1.0
                        stepSize: 0.02
                        value: root.info && root.info.temperature !== undefined ? root.info.temperature : 0.0
                        Layout.fillWidth: true
                        onApply: (v) => applyColor(root.info.brightness, root.info.contrast, root.info.saturation, v, root.info.tint)
                    }

                    StudioSlider {
                        label: "Tint"
                        from: -1.0
                        to: 1.0
                        stepSize: 0.02
                        value: root.info && root.info.tint !== undefined ? root.info.tint : 0.0
                        Layout.fillWidth: true
                        onApply: (v) => applyColor(root.info.brightness, root.info.contrast, root.info.saturation, root.info.temperature, v)
                    }

                    StudioButton {
                        text: "Reset Color"
                        iconText: "↺"
                        variant: "ghost"
                        Layout.fillWidth: true
                        onClicked: applyColor(0.0, 1.0, 1.0, 0.0, 0.0)
                    }
                }

                // 4. Transform & Geometry Card
                StudioCard {
                    title: "Transform & Geometry"
                    iconText: "📐"
                    collapsible: true
                    Layout.fillWidth: true

                    StudioSlider {
                        label: "Scale"
                        from: 0.1
                        to: 8.0
                        stepSize: 0.05
                        value: root.info ? root.info.scale : 1.0
                        unit: "×"
                        Layout.fillWidth: true
                        onApply: (v) => applyTransform({scale: v})
                    }

                    StudioSlider {
                        label: "Position X"
                        from: -1920
                        to: 1920
                        stepSize: 1
                        value: root.info ? root.info.x : 0
                        decimals: 0
                        unit: "px"
                        Layout.fillWidth: true
                        onApply: (v) => applyTransform({x: v})
                    }

                    StudioSlider {
                        label: "Position Y"
                        from: -1080
                        to: 1080
                        stepSize: 1
                        value: root.info ? root.info.y : 0
                        decimals: 0
                        unit: "px"
                        Layout.fillWidth: true
                        onApply: (v) => applyTransform({y: v})
                    }

                    StudioSlider {
                        label: "Rotation"
                        from: -180
                        to: 180
                        stepSize: 1
                        value: root.info ? root.info.rotation : 0
                        decimals: 0
                        unit: "°"
                        Layout.fillWidth: true
                        onApply: (v) => applyTransform({rotation: v})
                    }
                }

                // 5. Applied Effects Stack Card
                StudioCard {
                    title: "Applied Effects"
                    iconText: "✨"
                    collapsible: true
                    Layout.fillWidth: true
                    visible: root.info && root.info.effects && root.info.effects.length > 0

                    Repeater {
                        model: root.info ? root.info.effects : []
                        delegate: Rectangle {
                            id: outerDelegate
                            property int effectIndex: index
                            property string effectType: modelData.type
                            Layout.fillWidth: true
                            implicitHeight: effCol.implicitHeight + 12
                            color: Theme.bgElevated
                            radius: Theme.radiusSmall
                            border.color: Theme.borderMedium
                            border.width: 1

                            ColumnLayout {
                                id: effCol
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true

                                    Text {
                                        text: outerDelegate.effectType.toUpperCase()
                                        font.family: Theme.fontMono
                                        font.pixelSize: 11
                                        font.bold: true
                                        color: Theme.cyan
                                        Layout.fillWidth: true
                                    }

                                    StudioButton {
                                        text: "✕"
                                        compact: true
                                        variant: "danger"
                                        onClicked: session.removeClipEffect(selection.selectedClipId, outerDelegate.effectIndex)
                                    }
                                }

                                // Numerical Parameters
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
                                    delegate: StudioSlider {
                                        label: modelData.name
                                        from: (modelData.name === "intensity" || modelData.name === "smoothness" || modelData.name === "similarity") ? 0.0 : (modelData.name === "radius" && outerDelegate.effectType === "blur" ? 1.0 : 0.0)
                                        to: (modelData.name === "radius" && outerDelegate.effectType === "blur") ? 50.0 : (modelData.name === "amount" ? 5.0 : 2.0)
                                        stepSize: 0.02
                                        value: modelData.val
                                        Layout.fillWidth: true
                                        onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, outerDelegate.effectIndex, modelData.name, v)
                                    }
                                }

                                // String Parameters (e.g. key_color)
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
                                        spacing: 6

                                        Text {
                                            text: modelData.name
                                            font.family: Theme.fontBody
                                            font.pixelSize: 10
                                            color: Theme.textSecondary
                                            Layout.preferredWidth: 60
                                        }

                                        Rectangle {
                                            Layout.fillWidth: true
                                            height: 24
                                            radius: 4
                                            color: Theme.bgApp
                                            border.color: Theme.borderMedium
                                            border.width: 1

                                            TextInput {
                                                anchors.fill: parent
                                                anchors.margins: 4
                                                font.family: Theme.fontMono
                                                font.pixelSize: 10
                                                color: Theme.textPrimary
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

                // 6. Typography & Text Settings Card
                StudioCard {
                    title: "Typography & Text"
                    iconText: "🔤"
                    collapsible: true
                    Layout.fillWidth: true
                    visible: root.info && (root.info.kind === "text" || root.info.text !== "")

                    Text {
                        text: "Caption Text"
                        font.family: Theme.fontBody
                        font.pixelSize: 10
                        color: Theme.textSecondary
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 32
                        radius: Theme.radiusSmall
                        color: Theme.bgElevated
                        border.color: titleText.activeFocus ? Theme.borderFocus : Theme.borderMedium
                        border.width: 1

                        TextInput {
                            id: titleText
                            anchors.fill: parent
                            anchors.margins: 6
                            font.family: Theme.fontBody
                            font.pixelSize: 12
                            color: Theme.textPrimary
                            selectByMouse: true
                            text: root.info ? root.info.text : ""
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Rectangle {
                            Layout.fillWidth: true
                            height: 28
                            radius: 4
                            color: Theme.bgElevated
                            border.color: titleFont.activeFocus ? Theme.borderFocus : Theme.borderMedium
                            border.width: 1

                            TextInput {
                                id: titleFont
                                anchors.fill: parent
                                anchors.margins: 4
                                font.family: Theme.fontBody
                                font.pixelSize: 11
                                color: Theme.textPrimary
                                text: root.info && root.info.fontFamily !== "" ? root.info.fontFamily : "Arial"
                            }
                        }

                        SpinBox {
                            id: titleSize
                            from: 8
                            to: 400
                            value: root.info ? Math.round(root.info.fontSizePt) : 72
                        }
                    }

                    StudioButton {
                        text: "Apply Text Style"
                        iconText: "✓"
                        variant: "primary"
                        Layout.fillWidth: true
                        onClicked: session.setClipText(selection.selectedClipId, titleText.text, titleFont.text, titleSize.value)
                    }
                }
            }

            // ==================== TRANSITION INSPECTOR ====================
            ColumnLayout {
                visible: root.transInfo !== null && root.transInfo.id !== undefined
                spacing: 10
                Layout.fillWidth: true

                StudioCard {
                    title: "Transition: " + (root.transInfo ? root.transInfo.type : "")
                    iconText: "⚡"
                    Layout.fillWidth: true

                    Text {
                        text: "Type"
                        font.family: Theme.fontBody
                        font.pixelSize: 10
                        color: Theme.textSecondary
                    }

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

                    StudioSlider {
                        id: transDurSlider
                        label: "Duration"
                        from: 0.1
                        to: 10.0
                        stepSize: 0.1
                        value: root.transInfo ? root.transInfo.durationSec : 1.0
                        unit: "s"
                        Layout.fillWidth: true
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

                    Text {
                        text: "Alignment"
                        font.family: Theme.fontBody
                        font.pixelSize: 10
                        color: Theme.textSecondary
                    }

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

                    StudioButton {
                        text: "Delete Transition"
                        iconText: "🗑"
                        variant: "danger"
                        Layout.fillWidth: true
                        onClicked: {
                            if (selection.selectedTransitionId !== "") {
                                session.removeTransition(selection.selectedTransitionId)
                                selection.clearSelection()
                            }
                        }
                    }
                }
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
}
