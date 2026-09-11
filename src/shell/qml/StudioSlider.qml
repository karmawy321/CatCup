import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string label: ""
    property double from: 0.0
    property double to: 1.0
    property double stepSize: 0.01
    property double value: 0.0
    property string unit: ""
    property int decimals: 2

    signal apply(double v)

    implicitWidth: 200
    implicitHeight: label !== "" ? 44 : 26

    ColumnLayout {
        anchors.fill: parent
        spacing: 4

        RowLayout {
            visible: root.label !== ""
            Layout.fillWidth: true

            Text {
                text: root.label
                font.family: Theme.fontBody
                font.pixelSize: 11
                color: Theme.textSecondary
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Rectangle {
                implicitWidth: 64
                implicitHeight: 20
                radius: 4
                color: Theme.bgElevated
                border.color: numInput.activeFocus ? Theme.borderFocus : Theme.borderMedium
                border.width: 1

                TextInput {
                    id: numInput
                    anchors.fill: parent
                    anchors.leftMargin: 4
                    anchors.rightMargin: 4
                    verticalAlignment: TextInput.AlignVCenter
                    horizontalAlignment: TextInput.AlignRight
                    font.family: Theme.fontMono
                    font.pixelSize: 11
                    color: Theme.textPrimary
                    selectByMouse: true
                    text: Number(root.value).toFixed(root.decimals) + (root.unit !== "" ? " " + root.unit : "")

                    onEditingFinished: {
                        var parsed = parseFloat(text)
                        if (!isNaN(parsed)) {
                            var clamped = Math.max(root.from, Math.min(root.to, parsed))
                            root.value = clamped
                            root.apply(clamped)
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
            implicitHeight: 20

            // Slider Track Background
            Rectangle {
                id: trackBg
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: 4
                radius: 2
                color: Theme.bgActive

                // Filled Track
                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    radius: 2
                    width: Math.max(0, Math.min(trackBg.width, (thumb.x + thumb.width / 2)))
                    color: Theme.accent
                }
            }

            // Slider Thumb
            Rectangle {
                id: thumb
                width: dragArea.pressed ? 14 : 12
                height: width
                radius: width / 2
                y: (parent.height - height) / 2
                x: {
                    var range = root.to - root.from
                    if (range <= 0) return 0
                    var ratio = (root.value - root.from) / range
                    return ratio * (parent.width - width)
                }
                color: "#FFFFFF"
                border.color: Theme.accent
                border.width: 2

                Behavior on width { NumberAnimation { duration: 80 } }
            }

            MouseArea {
                id: dragArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                function updateFromPos(mouseX) {
                    var avail = width - thumb.width
                    if (avail <= 0) return
                    var ratio = Math.max(0.0, Math.min(1.0, mouseX / avail))
                    var rawVal = root.from + ratio * (root.to - root.from)
                    if (root.stepSize > 0) {
                        rawVal = Math.round(rawVal / root.stepSize) * root.stepSize
                    }
                    root.value = Math.max(root.from, Math.min(root.to, rawVal))
                }

                onPressed: (mouse) => updateFromPos(mouse.x)
                onPositionChanged: (mouse) => {
                    if (pressed) updateFromPos(mouse.x)
                }
                onReleased: root.apply(root.value)
            }
        }
    }
}
