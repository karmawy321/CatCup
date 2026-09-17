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
    property var transInfo: (selection.selectedTransitionId !== "" && typeof timeline.transitionInfo === 'function')
        ? timeline.transitionInfo(selection.selectedTransitionId) : null

    function refresh() {
        info = (selection && selection.selectedClipId !== "")
            ? timeline.clipInfo(selection.selectedClipId) : null
        transInfo = (selection && selection.selectedTransitionId !== "" && typeof timeline.transitionInfo === 'function')
            ? timeline.transitionInfo(selection.selectedTransitionId) : null
    }

    Connections {
        target: selection
        function onSelectedClipIdChanged() { root.refresh() }
        function onSelectedTransitionIdChanged() { root.refresh() }
    }

    Connections {
        target: timeline
        function onModelChanged() { root.refresh() }
    }

    Connections {
        target: session
        function onProjectChanged() { root.refresh() }
    }

    property int activeTab: 0 // 0: Details, 1: Video, 2: Audio, 3: Speed, 4: Adjustment
    property var mainWindow: null

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
                            { name: "Adjustment", id: 4 },
                            { name: "Effects", id: 5 }
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
            id: inspectorScrollView
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: Math.max(100, inspectorScrollView.availableWidth - 20)
                x: 10
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
                                    text: (session ? session.sequenceWidth() + "×" + session.sequenceHeight() : "1920×1080") + " (" + ((session && typeof session.getSequenceAspectPreset === 'function') ? session.getSequenceAspectPreset() : "16:9") + ")"
                                    font.family: Theme.fontMono; font.pixelSize: 11; color: Theme.textPrimary
                                    Layout.fillWidth: true
                                }
                            }

                            // Frame rate Row
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "Frame rate"; font.family: Theme.fontBody; font.pixelSize: 11; color: Theme.textTertiary; Layout.preferredWidth: 80 }
                                Text {
                                    text: (session ? session.sequenceFps().toFixed(2) : "30.00") + " fps"
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
                                seqDialog.open()
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

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text {
                                text: "Blend Mode"
                                font.family: Theme.fontBody
                                font.pixelSize: 11
                                color: Theme.textSecondary
                                Layout.preferredWidth: 80
                            }
                            ComboBox {
                                id: blendCombo
                                Layout.fillWidth: true
                                model: ["Normal", "Screen", "Multiply", "Overlay", "Darken", "Lighten", "Color Burn", "Linear Burn", "Color Dodge", "Soft Light"]
                                readonly property var modeValues: ["normal", "screen", "multiply", "overlay", "darken", "lighten", "color_burn", "linear_burn", "color_dodge", "soft_light"]
                                currentIndex: {
                                    if (!root.info || !root.info.blendMode) return 0
                                    var idx = modeValues.indexOf(root.info.blendMode)
                                    return idx >= 0 ? idx : 0
                                }
                                onActivated: (idx) => {
                                    if (selection.selectedClipId !== "") {
                                        session.setClipBlendMode(selection.selectedClipId, modeValues[idx])
                                    }
                                }
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

                    // Extract Audio Quick Action in Video Tab
                    StudioCard {
                        visible: root.info !== null && root.info.kind === "video" && (typeof session.clipHasAudio !== 'function' || session.clipHasAudio(selection.selectedClipId))
                        title: "Audio Extraction"
                        iconText: "♫"
                        Layout.fillWidth: true

                        Text {
                            text: "Extract and separate the audio stream onto an independent audio track on the timeline."
                            font.family: Theme.fontBody
                            font.pixelSize: 10
                            color: Theme.textTertiary
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        StudioButton {
                            text: "Extract Audio to Track (A1)"
                            iconText: "♫⇥"
                            variant: "primary"
                            Layout.fillWidth: true
                            onClicked: {
                                if (selection.selectedClipId !== "") {
                                    session.extractAudioFromClip(selection.selectedClipId)
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
                        title: "Audio Extraction & Separation"
                        iconText: "♫"
                        Layout.fillWidth: true

                        Text {
                            text: "Decouple video audio onto independent timeline tracks or export to pristine 48kHz WAV audio."
                            font.family: Theme.fontBody
                            font.pixelSize: 10
                            color: Theme.textTertiary
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        StudioButton {
                            text: "Extract Audio to Timeline"
                            iconText: "♫⇥"
                            variant: "primary"
                            Layout.fillWidth: true
                            enabled: selection.selectedClipId !== "" && (typeof session.clipHasAudio !== 'function' || session.clipHasAudio(selection.selectedClipId))
                            onClicked: {
                                if (selection.selectedClipId !== "") {
                                    session.extractAudioFromClip(selection.selectedClipId)
                                }
                            }
                        }

                        StudioButton {
                            text: "Export Audio to WAV File"
                            iconText: "⇣"
                            variant: "secondary"
                            Layout.fillWidth: true
                            enabled: selection.selectedClipId !== "" && (typeof session.clipHasAudio !== 'function' || session.clipHasAudio(selection.selectedClipId))
                            onClicked: {
                                if (selection.selectedClipId !== "") {
                                    session.extractAudioToFile(selection.selectedClipId)
                                }
                            }
                        }
                    }

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
                            onClicked: {
                                applyColor(0.0, 1.0, 1.0, 0.0, 0.0)
                            }
                        }
                    }

                    // Highlights & Shadows (CapCut Curve Adjustments)
                    StudioCard {
                        visible: root.info !== null && root.info.clipId !== undefined
                        title: "Highlights & Shadows"
                        iconText: "◐"
                        Layout.fillWidth: true

                        StudioSlider {
                            label: "Highlights"
                            from: -1.0
                            to: 1.0
                            stepSize: 0.02
                            value: root.info && root.info.highlights !== undefined ? root.info.highlights : 0.0
                            decimals: 2
                            Layout.fillWidth: true
                            onApply: (v) => {
                                var s = root.info && root.info.shadows !== undefined ? root.info.shadows : 0.0
                                session.setClipHighlightsShadows(selection.selectedClipId, v, s)
                            }
                        }

                        StudioSlider {
                            label: "Shadows"
                            from: -1.0
                            to: 1.0
                            stepSize: 0.02
                            value: root.info && root.info.shadows !== undefined ? root.info.shadows : 0.0
                            decimals: 2
                            Layout.fillWidth: true
                            onApply: (v) => {
                                var h = root.info && root.info.highlights !== undefined ? root.info.highlights : 0.0
                                session.setClipHighlightsShadows(selection.selectedClipId, h, v)
                            }
                        }
                    }

                    // Primary Color Wheels (CapCut primary_wheel_v1)
                    StudioCard {
                        visible: root.info !== null && root.info.clipId !== undefined
                        title: "Primary Color Wheels"
                        iconText: "◎"
                        Layout.fillWidth: true

                        StudioSlider {
                            label: "Lift Y"
                            from: -1.0
                            to: 1.0
                            stepSize: 0.02
                            value: root.info && root.info.liftY !== undefined ? root.info.liftY : 0.0
                            decimals: 2
                            Layout.fillWidth: true
                            onApply: (v) => applyWheels(v, undefined, undefined, undefined)
                        }

                        StudioSlider {
                            label: "Gamma Y"
                            from: 0.1
                            to: 4.0
                            stepSize: 0.02
                            value: root.info && root.info.gammaY !== undefined ? root.info.gammaY : 1.0
                            decimals: 2
                            Layout.fillWidth: true
                            onApply: (v) => applyWheels(undefined, v, undefined, undefined)
                        }

                        StudioSlider {
                            label: "Gain Y"
                            from: 0.0
                            to: 4.0
                            stepSize: 0.05
                            value: root.info && root.info.gainY !== undefined ? root.info.gainY : 1.0
                            decimals: 2
                            Layout.fillWidth: true
                            onApply: (v) => applyWheels(undefined, undefined, v, undefined)
                        }

                        StudioSlider {
                            label: "Luma Mix"
                            from: 0.0
                            to: 1.0
                            stepSize: 0.02
                            value: root.info && root.info.lumaMix !== undefined ? root.info.lumaMix : 1.0
                            decimals: 2
                            Layout.fillWidth: true
                            onApply: (v) => applyWheels(undefined, undefined, undefined, v)
                        }
                    }
                }

                // ==================== 5. EFFECTS TAB (Applied Clip Effects Stack) ====================
                ColumnLayout {
                    visible: root.activeTab === 5
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
                            text: "Select a video or image clip on the timeline"
                            font.family: Theme.fontBody
                            font.pixelSize: 11
                            color: Theme.textSecondary
                        }
                    }

                    // Empty Effects Notice
                    Rectangle {
                        visible: root.info !== null && root.info.clipId !== undefined && (!root.info.effects || root.info.effects.length === 0)
                        Layout.fillWidth: true
                        height: 100
                        radius: 6
                        color: Theme.bgCard
                        border.color: Theme.borderMedium
                        border.width: 1

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 6
                            Text {
                                text: "✦ No Effects Applied"
                                font.family: Theme.fontBody
                                font.pixelSize: 12
                                font.bold: true
                                color: Theme.textPrimary
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Text {
                                text: "Add cinematic effects from the Effects library in the left browser."
                                font.family: Theme.fontBody
                                font.pixelSize: 10
                                color: Theme.textTertiary
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                    }

                    // Repeater for all applied effects on this clip
                    Repeater {
                        model: (root.info && root.info.effects) ? root.info.effects : []
                        delegate: StudioCard {
                            id: effCard
                            Layout.fillWidth: true
                            readonly property int effIdx: index
                            readonly property var effData: modelData
                            readonly property string effType: modelData.type || ""
                            title: {
                                if (effType === "letterbox") return "Letterbox 2.35:1"
                                if (effType === "film_grain") return "35mm Film Grain"
                                if (effType === "chromatic_aberration") return "Chromatic Aberration"
                                if (effType === "bloom") return "Cinematic Bloom"
                                if (effType === "split_toning") return "Split Toning (Teal & Orange)"
                                if (effType === "retro_vhs") return "Retro VHS Tape"
                                if (effType === "posterize") return "Posterize"
                                if (effType === "invert") return "Invert Negative"
                                if (effType === "edge_detect") return "Edge Detect"
                                if (effType === "mirror") return "Mirror Reflection"
                                if (effType === "vignette") return "Vignette"
                                if (effType === "blur") return "Blur"
                                if (effType === "sharpen") return "Sharpen"
                                if (effType === "chroma_key") return "Chroma Key"
                                return effType
                            }
                            iconText: "✦"

                            RowLayout {
                                Layout.fillWidth: true
                                Item { Layout.fillWidth: true }
                                StudioButton {
                                    text: "Remove"
                                    iconText: "×"
                                    variant: "danger"
                                    compact: true
                                    onClicked: {
                                        if (selection.selectedClipId !== "") {
                                            session.removeClipEffect(selection.selectedClipId, effIdx)
                                        }
                                    }
                                }
                            }

                            // Letterbox
                            ColumnLayout {
                                visible: effType === "letterbox"
                                Layout.fillWidth: true
                                spacing: 8
                                StudioSlider {
                                    label: "Bar Height"
                                    from: 0.0
                                    to: 0.4
                                    stepSize: 0.01
                                    value: effData.params && effData.params.barHeight !== undefined ? effData.params.barHeight : 0.12
                                    unit: "%"
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "barHeight", v)
                                }
                                StudioSlider {
                                    label: "Edge Feather"
                                    from: 0.0
                                    to: 0.1
                                    stepSize: 0.005
                                    value: effData.params && effData.params.feather !== undefined ? effData.params.feather : 0.0
                                    unit: "%"
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "feather", v)
                                }
                            }

                            // Film Grain
                            ColumnLayout {
                                visible: effType === "film_grain"
                                Layout.fillWidth: true
                                spacing: 8
                                StudioSlider {
                                    label: "Intensity"
                                    from: 0.0
                                    to: 1.0
                                    stepSize: 0.02
                                    value: effData.params && effData.params.intensity !== undefined ? effData.params.intensity : 0.25
                                    unit: "%"
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "intensity", v)
                                }
                                StudioSlider {
                                    label: "Grain Size"
                                    from: 1.0
                                    to: 4.0
                                    stepSize: 0.5
                                    value: effData.params && effData.params.size !== undefined ? effData.params.size : 1.0
                                    unit: "px"
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "size", v)
                                }
                            }

                            // Chromatic Aberration
                            ColumnLayout {
                                visible: effType === "chromatic_aberration"
                                Layout.fillWidth: true
                                spacing: 8
                                StudioSlider {
                                    label: "Shift X"
                                    from: -30.0
                                    to: 30.0
                                    stepSize: 1.0
                                    value: effData.params && effData.params.shiftX !== undefined ? effData.params.shiftX : 5.0
                                    unit: "px"
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "shiftX", v)
                                }
                                StudioSlider {
                                    label: "Shift Y"
                                    from: -30.0
                                    to: 30.0
                                    stepSize: 1.0
                                    value: effData.params && effData.params.shiftY !== undefined ? effData.params.shiftY : 0.0
                                    unit: "px"
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "shiftY", v)
                                }
                            }

                            // Cinematic Bloom
                            ColumnLayout {
                                visible: effType === "bloom"
                                Layout.fillWidth: true
                                spacing: 8
                                StudioSlider {
                                    label: "Threshold"
                                    from: 0.2
                                    to: 0.95
                                    stepSize: 0.02
                                    value: effData.params && effData.params.threshold !== undefined ? effData.params.threshold : 0.65
                                    unit: "%"
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "threshold", v)
                                }
                                StudioSlider {
                                    label: "Radius"
                                    from: 2.0
                                    to: 40.0
                                    stepSize: 1.0
                                    value: effData.params && effData.params.radius !== undefined ? effData.params.radius : 12.0
                                    unit: "px"
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "radius", v)
                                }
                                StudioSlider {
                                    label: "Glow Intensity"
                                    from: 0.0
                                    to: 2.0
                                    stepSize: 0.05
                                    value: effData.params && effData.params.intensity !== undefined ? effData.params.intensity : 0.7
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "intensity", v)
                                }
                            }

                            // Split Toning
                            ColumnLayout {
                                visible: effType === "split_toning"
                                Layout.fillWidth: true
                                spacing: 8
                                StudioSlider {
                                    label: "Shadow Teal"
                                    from: 0.0
                                    to: 1.0
                                    stepSize: 0.02
                                    value: effData.params && effData.params.shadowTeal !== undefined ? effData.params.shadowTeal : 0.4
                                    unit: "%"
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "shadowTeal", v)
                                }
                                StudioSlider {
                                    label: "Highlight Orange"
                                    from: 0.0
                                    to: 1.0
                                    stepSize: 0.02
                                    value: effData.params && effData.params.highlightOrange !== undefined ? effData.params.highlightOrange : 0.4
                                    unit: "%"
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "highlightOrange", v)
                                }
                            }

                            // Retro VHS
                            ColumnLayout {
                                visible: effType === "retro_vhs"
                                Layout.fillWidth: true
                                spacing: 8
                                StudioSlider {
                                    label: "Scanlines"
                                    from: 0.0
                                    to: 1.0
                                    stepSize: 0.02
                                    value: effData.params && effData.params.scanlines !== undefined ? effData.params.scanlines : 0.35
                                    unit: "%"
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "scanlines", v)
                                }
                                StudioSlider {
                                    label: "Color Bleed"
                                    from: 0.0
                                    to: 15.0
                                    stepSize: 1.0
                                    value: effData.params && effData.params.colorBleed !== undefined ? effData.params.colorBleed : 3.0
                                    unit: "px"
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "colorBleed", v)
                                }
                            }

                            // Posterize
                            ColumnLayout {
                                visible: effType === "posterize"
                                Layout.fillWidth: true
                                spacing: 8
                                StudioSlider {
                                    label: "Levels"
                                    from: 2.0
                                    to: 32.0
                                    stepSize: 1.0
                                    value: effData.params && effData.params.levels !== undefined ? effData.params.levels : 6.0
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "levels", v)
                                }
                            }

                            // Invert
                            ColumnLayout {
                                visible: effType === "invert"
                                Layout.fillWidth: true
                                spacing: 8
                                StudioSlider {
                                    label: "Intensity"
                                    from: 0.0
                                    to: 1.0
                                    stepSize: 0.02
                                    value: effData.params && effData.params.intensity !== undefined ? effData.params.intensity : 1.0
                                    unit: "%"
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "intensity", v)
                                }
                            }

                            // Edge Detect
                            ColumnLayout {
                                visible: effType === "edge_detect"
                                Layout.fillWidth: true
                                spacing: 8
                                StudioSlider {
                                    label: "Intensity"
                                    from: 0.1
                                    to: 3.0
                                    stepSize: 0.1
                                    value: effData.params && effData.params.intensity !== undefined ? effData.params.intensity : 1.0
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "intensity", v)
                                }
                            }

                            // Mirror Reflection
                            ColumnLayout {
                                visible: effType === "mirror"
                                Layout.fillWidth: true
                                spacing: 8
                                RowLayout {
                                    Layout.fillWidth: true
                                    Text { text: "Mirror Mode"; font.family: Theme.fontBody; font.pixelSize: 11; color: Theme.textSecondary; Layout.preferredWidth: 80 }
                                    ComboBox {
                                        Layout.fillWidth: true
                                        model: ["Horizontal Flip", "Vertical Flip", "Center Mirror H", "Center Mirror V", "Quad Kaleidoscope"]
                                        currentIndex: effData.params && effData.params.mode !== undefined ? Math.floor(effData.params.mode) : 0
                                        onActivated: (idx) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "mode", idx)
                                    }
                                }
                            }

                            // Vignette
                            ColumnLayout {
                                visible: effType === "vignette"
                                Layout.fillWidth: true
                                spacing: 8
                                StudioSlider {
                                    label: "Intensity"
                                    from: 0.0
                                    to: 1.0
                                    stepSize: 0.02
                                    value: effData.params && effData.params.intensity !== undefined ? effData.params.intensity : 0.5
                                    unit: "%"
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "intensity", v)
                                }
                                StudioSlider {
                                    label: "Radius"
                                    from: 0.1
                                    to: 1.5
                                    stepSize: 0.02
                                    value: effData.params && effData.params.radius !== undefined ? effData.params.radius : 0.7
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "radius", v)
                                }
                            }

                            // Blur
                            ColumnLayout {
                                visible: effType === "blur"
                                Layout.fillWidth: true
                                spacing: 8
                                StudioSlider {
                                    label: "Radius"
                                    from: 1.0
                                    to: 50.0
                                    stepSize: 1.0
                                    value: effData.params && effData.params.radius !== undefined ? effData.params.radius : 10.0
                                    unit: "px"
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "radius", v)
                                }
                            }

                            // Sharpen
                            ColumnLayout {
                                visible: effType === "sharpen"
                                Layout.fillWidth: true
                                spacing: 8
                                StudioSlider {
                                    label: "Amount"
                                    from: 0.1
                                    to: 2.0
                                    stepSize: 0.05
                                    value: effData.params && effData.params.amount !== undefined ? effData.params.amount : 0.5
                                    Layout.fillWidth: true
                                    onApply: (v) => session.updateClipEffectParam(selection.selectedClipId, effIdx, "amount", v)
                                }
                            }
                        }
                    }
                }

                // ==================== TRANSITION INSPECTOR ====================
                ColumnLayout {
                    visible: root.transInfo && root.transInfo.id !== undefined
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
                            model: ["crossfade", "dip_black", "dip_white", "iris_circle", "barn_doors_h", "barn_doors_v", "zoom_in", "zoom_out", "flash_dissolve", "wipe_left", "wipe_right", "wipe_up", "wipe_down"]
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
        if (typeof session.setClipColorAdjust === 'function' && selection.selectedClipId !== "") {
            session.setClipColorAdjust(selection.selectedClipId, b, c, s, temp, tint)
        }
    }

    function applyWheels(ly, gy, gny, lm) {
        if (selection.selectedClipId !== "") {
            var i = root.info || {}
            var currLy = (ly !== undefined) ? ly : (i.liftY !== undefined ? i.liftY : 0.0)
            var currGy = (gy !== undefined) ? gy : (i.gammaY !== undefined ? i.gammaY : 1.0)
            var currGny = (gny !== undefined) ? gny : (i.gainY !== undefined ? i.gainY : 1.0)
            var currLm = (lm !== undefined) ? lm : (i.lumaMix !== undefined ? i.lumaMix : 1.0)
            session.setClipColorWheels(
                selection.selectedClipId,
                currLy,
                currGy,
                currGny,
                i.offsetR !== undefined ? i.offsetR : 0.0,
                i.offsetG !== undefined ? i.offsetG : 0.0,
                i.offsetB !== undefined ? i.offsetB : 0.0,
                currLm
            )
        }
    }

    Dialog {
        id: seqDialog
        title: "Sequence Settings"
        modal: true
        anchors.centerIn: parent
        width: 320
        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: "Resolution & Aspect Ratio"
                font.family: Theme.fontBody
                font.pixelSize: 11
                color: Theme.textSecondary
            }
            ComboBox {
                id: resCombo
                Layout.fillWidth: true
                model: ["1920×1080 (16:9 Landscape)", "1080×1920 (9:16 Portrait)", "3840×2160 (4K UHD)", "1280×720 (720p HD)", "1080×1080 (1:1 Square)"]
                currentIndex: {
                    var w = session ? session.sequenceWidth() : 1920
                    var h = session ? session.sequenceHeight() : 1080
                    if (w === 1080 && h === 1920) return 1
                    if (w === 3840 && h === 2160) return 2
                    if (w === 1280 && h === 720) return 3
                    if (w === 1080 && h === 1080) return 4
                    return 0
                }
            }
        }
        onAccepted: {
            if (resCombo.currentIndex === 0) session.setSequenceFormat(1920, 1080)
            else if (resCombo.currentIndex === 1) session.setSequenceFormat(1080, 1920)
            else if (resCombo.currentIndex === 2) session.setSequenceFormat(3840, 2160)
            else if (resCombo.currentIndex === 3) session.setSequenceFormat(1280, 720)
            else if (resCombo.currentIndex === 4) session.setSequenceFormat(1080, 1080)
        }
    }
}
