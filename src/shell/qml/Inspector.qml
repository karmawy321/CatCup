import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// CapCut Desktop 1:1 Inspector / Property Panel:
// - Top Tab Bar: Details | Video | Audio | Speed | Animation | Adjustment
// - Details view matching CapCut screenshot: Name, Path, Color space (Rec. 709 SDR), Size, FPS, and [Modify] button
// - Video view: Transform (Scale, Pos X, Pos Y, Rot), Opacity, Fade In/Out, Keyframes (+ Keyframe at Playhead)
// - Audio view: Loudness Normalization (ITU-R BS.1770 -14 LUFS) & Voice Cleanup (Denoise)
// - Speed view: Retiming slider with 0.5x, 1.0x, 2.0x, 4.0x presets
// - Adjustment view: Color Grading (Temp, Tint, Saturation, Brightness, Contrast)

Rectangle {
    id: root
    color: Theme.bgSidebar
    border.color: Theme.borderSubtle
    border.width: 1

    property var info: selection.selectedClipId !== ""
        ? timeline.clipInfo(selection.selectedClipId) : null
    property var transInfo: selection.selectedTransitionId !== ""
        ? timeline.transitionInfo(selection.selectedTransitionId) : null

    property int activeTab: 0 // 0: Details, 1: Video, 2: Audio, 3: Speed, 4: Adjustment

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ---- CapCut Top Tab Bar ----
        Rectangle {
            Layout.fillWidth: true
            height: 38
            color: Theme.bgSurface
            border.color: Theme.borderSubtle
            border.width: 1

            ScrollView {
                anchors.fill: parent
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ScrollBar.vertical.policy: ScrollBar.AlwaysOff

                Row {
                    spacing: 4
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8

                    Repeater {
                        model: [
                            { name: "Details", id: 0 },
                            { name: "Video", id: 1 },
                            { name: "Audio", id: 2 },
                            { name: "Speed", id: 3 },
                            { name: "Adjustment", id: 4 }
                        ]

                        delegate: Rectangle {
                            implicitWidth: tabText.implicitWidth + 16
                            implicitHeight: 38
                            color: "transparent"

                            Text {
                                id: tabText
                                anchors.centerIn: parent
                                text: modelData.name
                                font.family: Theme.fontBody
                                font.pixelSize: 11
                                font.weight: root.activeTab === modelData.id ? Font.DemiBold : Font.Normal
                                color: root.activeTab === modelData.id ? Theme.accent : Theme.textSecondary
                            }

                            // Active Cyan Underline
                            Rectangle {
                                visible: root.activeTab === modelData.id
                                anchors.bottom: parent.bottom
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.leftMargin: 6
                                anchors.rightMargin: 6
                                height: 2
                                color: Theme.accent
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.activeTab = modelData.id
                            }
                        }
                    }
                }
            }
        }

        // ---- Tab Body Content ----
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            anchors.margins: 10
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: root.availableWidth - 20
                spacing: 12

                // ==================== 0. DETAILS VIEW (Exact CapCut Replication) ====================
                ColumnLayout {
                    visible: root.activeTab === 0
                    Layout.fillWidth: true
                    spacing: 10

                    // Clip / Sequence Details Card
                    Rectangle {
                        Layout.fillWidth: true
                        radius: 6
                        color: Theme.bgCard
                        border.color: Theme.borderMedium
                        border.width: 1
                        implicitHeight: detailsCol.implicitHeight + 24

                        ColumnLayout {
                            id: detailsCol
                            anchors.fill: parent
                            anchors.margins: 12
                            spacing: 10

                            // Name Row
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "Name"; font.family: Theme.fontBody; font.pixelSize: 11; color: Theme.textTertiary; Layout.preferredWidth: 80 }
                                Text {
                                    text: root.info ? root.info.name : (session ? session.projectName : "Timeline 01")
                                    font.family: Theme.fontBody; font.pixelSize: 11; font.weight: Font.DemiBold; color: Theme.textPrimary
                                    elide: Text.ElideRight; Layout.fillWidth: true
                                }
                            }

                            // Path Row
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "Path"; font.family: Theme.fontBody; font.pixelSize: 11; color: Theme.textTertiary; Layout.preferredWidth: 80 }
                                Text {
                                    text: root.info ? "Source Media (" + root.info.kind + ")" : "Project File"
                                    font.family: Theme.fontMono; font.pixelSize: 10; color: Theme.textSecondary
                                    elide: Text.ElideRight; Layout.fillWidth: true
                                }
                            }

                            // Color space Row (Matches CapCut screenshot: "Rec. 709 SDR")
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "Color space"; font.family: Theme.fontBody; font.pixelSize: 11; color: Theme.textTertiary; Layout.preferredWidth: 80 }
                                Text {
                                    text: "Rec. 709 SDR"
                                    font.family: Theme.fontBody; font.pixelSize: 11; color: Theme.textPrimary
                                    Layout.fillWidth: true
                                }
                            }

                            // Size Row
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "Size"; font.family: Theme.fontBody; font.pixelSize: 11; color: Theme.textTertiary; Layout.preferredWidth: 80 }
                                Text {
                                    text: "1920×1080 (" + (session ? session.getSequenceAspectPreset() : "16:9") + ")"
                                    font.family: Theme.fontMono; font.pixelSize: 11; color: Theme.textPrimary
                                    Layout.fillWidth: true
                                }
                            }

                            // Frame rate Row
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "Frame rate"; font.family: Theme.fontBody; font.pixelSize: 11; color: Theme.textTertiary; Layout.preferredWidth: 80 }
                                Text {
                                    text: "30.00 fps"
                                    font.family: Theme.fontMono; font.pixelSize: 11; color: Theme.textPrimary
                                    Layout.fillWidth: true
                                }
                            }

                            // Duration Row
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "Duration"; font.family: Theme.fontBody; font.pixelSize: 11; color: Theme.textTertiary; Layout.preferredWidth: 80 }
                                Text {
                                    text: (root.info ? root.info.durationSec.toFixed(2) : timeline.durationSec.toFixed(2)) + "s"
                                    font.family: Theme.fontMono; font.pixelSize: 11; color: Theme.accent
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }

                    // Prominent [Modify] Button (As seen at bottom of CapCut Details panel)
                    Rectangle {
                        Layout.fillWidth: true
                        height: 32
                        radius: 16
                        color: modMa.containsMouse ? Theme.bgHover : Theme.bgElevated
                        border.color: Theme.borderMedium
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "Modify"
                            font.family: Theme.fontBody
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }

                        MouseArea {
                            id: modMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.info) root.activeTab = 1
                            }
                        }
                    }
                }

                // ==================== 1. VIDEO TAB (Transform, Opacity, Keyframes) ====================
                ColumnLayout {
                    visible: root.activeTab === 1
                    Layout.fillWidth: true
                    spacing: 10

                    // Empty Selection Notice
                    Rectangle {
                        visible: root.info === null || root.info.clipId === undefined
                        Layout.fillWidth: true
                        height: 80
                        radius: 6
                        color: Theme.bgCard
                        border.color: Theme.borderMedium
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "Select a video or text clip on the timeline"
                            font.family: Theme.fontBody
                            font.pixelSize: 11
                            color: Theme.textSecondary
                        }
                    }

                    // Basic Transform Card
                    StudioCard {
                        visible: root.info !== null && root.info.clipId !== undefined
                        title: "Basic Transform"
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

                    // Blend & Fades Card
                    StudioCard {
                        visible: root.info !== null && root.info.clipId !== undefined
                        title: "Blend & Fades"
                        Layout.fillWidth: true

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

                        StudioSlider {
                            label: "Fade In"
                            from: 0.0
                            to: 5.0
                            stepSize: 0.1
                            value: root.info && root.info.fadeInSec !== undefined ? root.info.fadeInSec : 0.0
                            unit: "s"
                            Layout.fillWidth: true
                            onApply: (v) => {
                                var out = root.info && root.info.fadeOutSec !== undefined ? root.info.fadeOutSec : 0.0
                                session.setClipFade(selection.selectedClipId, v, out)
                            }
                        }

                        StudioSlider {
                            label: "Fade Out"
                            from: 0.0
                            to: 5.0
                            stepSize: 0.1
                            value: root.info && root.info.fadeOutSec !== undefined ? root.info.fadeOutSec : 0.0
                            unit: "s"
                            Layout.fillWidth: true
                            onApply: (v) => {
                                var fi = root.info && root.info.fadeInSec !== undefined ? root.info.fadeInSec : 0.0
                                session.setClipFade(selection.selectedClipId, fi, v)
                            }
                        }
                    }

                    // Keyframe Animation Card (Stage 5 Feature!)
                    StudioCard {
                        visible: root.info !== null && root.info.clipId !== undefined
                        title: "Keyframe Animation"
                        iconText: "◆"
                        Layout.fillWidth: true

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                text: "Active Keyframes:"
                                font.family: Theme.fontBody
                                font.pixelSize: 11
                                color: Theme.textSecondary
                                Layout.fillWidth: true
                            }

                            Rectangle {
                                implicitWidth: 28
                                implicitHeight: 20
                                radius: 4
                                color: Theme.bgElevated
                                border.color: Theme.accent
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: root.info ? (root.info.keyframeCount || 0) : 0
                                    font.family: Theme.fontMono
                                    font.pixelSize: 10
                                    font.bold: true
                                    color: Theme.accent
                                }
                            }
                        }

                        StudioButton {
                            text: "+ Keyframe at Playhead"
                            iconText: "◆"
                            variant: "primary"
                            Layout.fillWidth: true
                            onClicked: {
                                if (root.info && selection.selectedClipId !== "") {
                                    session.setClipKeyframe(
                                        selection.selectedClipId,
                                        selection.playheadSec,
                                        root.info.scale,
                                        root.info.x,
                                        root.info.y,
                                        root.info.rotation,
                                        root.info.opacity,
                                        "easeInOut"
                                    )
                                }
                            }
                        }

                        StudioButton {
                            text: "− Remove Keyframe"
                            iconText: "×"
                            variant: "ghost"
                            Layout.fillWidth: true
                            onClicked: {
                                if (selection.selectedClipId !== "") {
                                    session.removeClipKeyframe(selection.selectedClipId, selection.playheadSec)
                                }
                            }
                        }
                    }
                }

                // ==================== 2. AUDIO TAB (LUFS Normalization & Voice Cleanup) ====================
                ColumnLayout {
                    visible: root.activeTab === 2
                    Layout.fillWidth: true
                    spacing: 10

                    StudioCard {
                        title: "Loudness & Dynamics"
                        iconText: "▲"
                        Layout.fillWidth: true

                        Text {
                            text: "ITU-R BS.1770 / EBU R128 Loudness Normalization"
                            font.family: Theme.fontBody
                            font.pixelSize: 10
                            color: Theme.textTertiary
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        StudioButton {
                            text: "Normalize Audio (-14 LUFS)"
                            iconText: "▲"
                            variant: "primary"
                            Layout.fillWidth: true
                            onClicked: {
                                if (selection.selectedClipId !== "") {
                                    session.normalizeClipAudio(selection.selectedClipId, -14.0)
                                }
                            }
                        }
                    }

                    StudioCard {
                        title: "Voice Cleanup & Denoise"
                        iconText: "✦"
                        Layout.fillWidth: true

                        Text {
                            text: "High-pass rumble filter (80 Hz) and speech noise gate"
                            font.family: Theme.fontBody
                            font.pixelSize: 10
                            color: Theme.textTertiary
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        StudioButton {
                            text: "Apply Voice Cleanup"
                            iconText: "✓"
                            variant: "secondary"
                            Layout.fillWidth: true
                            onClicked: {
                                if (selection.selectedClipId !== "") {
                                    session.denoiseClipAudio(selection.selectedClipId, 80.0)
                                }
                            }
                        }
                    }
                }

                // ==================== 3. SPEED TAB (Retiming & Presets) ====================
                ColumnLayout {
                    visible: root.activeTab === 3
                    Layout.fillWidth: true
                    spacing: 10

                    StudioCard {
                        title: "Playback Speed"
                        iconText: "◷"
                        Layout.fillWidth: true

                        StudioSlider {
                            label: "Speed Factor"
                            from: 0.1
                            to: 10.0
                            stepSize: 0.1
                            value: root.info && root.info.speed !== undefined ? root.info.speed : 1.0
                            unit: "×"
                            Layout.fillWidth: true
                            onApply: (v) => session.setClipSpeed(selection.selectedClipId, Math.max(0.1, Math.min(10.0, v)))
                        }

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
                }

                // ==================== 4. ADJUSTMENT / COLOR TAB ====================
                ColumnLayout {
                    visible: root.activeTab === 4
                    Layout.fillWidth: true
                    spacing: 10

                    StudioCard {
                        title: "Color Grading"
                        iconText: "◈"
                        Layout.fillWidth: true


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

                        StudioButton {
                            text: "Reset Color"
                            iconText: "↺"
                            variant: "ghost"
                            Layout.fillWidth: true
                            onClicked: applyColor(0.0, 1.0, 1.0, 0.0, 0.0)
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

                        StudioButton {
                            text: "Delete Transition"
                            iconText: "×"
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
