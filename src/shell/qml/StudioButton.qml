import QtQuick
import QtQuick.Controls

Item {
    id: root

    property string text: ""
    property string iconText: ""
    property string variant: "secondary" // "primary", "secondary", "ghost", "danger", "accent"
    property bool checkable: false
    property bool checked: false
    property bool compact: false
    property bool enabled: true

    signal clicked()

    implicitWidth: Math.max(compact ? 30 : 36, contentRow.implicitWidth + (compact ? 16 : 24))
    implicitHeight: compact ? 26 : 34

    opacity: enabled ? 1.0 : 0.45

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: root.compact ? 4 : 6

        color: {
            if (root.variant === "primary") {
                if (mouseArea.pressed) return Theme.accentPressed
                if (mouseArea.containsMouse) return Theme.accentHover
                return Theme.accent
            }
            if (root.checked) {
                return Theme.bgActive
            }
            if (root.variant === "ghost") {
                if (mouseArea.pressed) return Theme.bgActive
                if (mouseArea.containsMouse) return Theme.bgHover
                return "transparent"
            }
            if (root.variant === "danger") {
                if (mouseArea.pressed) return "#B91C1C"
                if (mouseArea.containsMouse) return "#DC2626"
                return "#2A1517"
            }
            // Secondary default
            if (mouseArea.pressed) return Theme.bgActive
            if (mouseArea.containsMouse) return Theme.bgHover
            return Theme.bgElevated
        }

        border.color: {
            if (root.variant === "primary") return "transparent"
            if (root.checked) return Theme.borderFocus
            if (root.variant === "accent") return Theme.accent
            if (mouseArea.containsMouse) return Theme.borderHighlight
            if (root.variant === "danger") return "#7F1D1D"
            if (root.variant === "ghost") return "transparent"
            return Theme.borderMedium
        }
        border.width: 1

        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 120 } }
    }

    Row {
        id: contentRow
        anchors.centerIn: parent
        spacing: (root.iconText !== "" && root.text !== "") ? 6 : 0

        Text {
            visible: root.iconText !== ""
            text: root.iconText
            font.pixelSize: root.compact ? 12 : 13
            font.bold: true
            color: {
                if (root.variant === "primary") return "#090A0D"
                if (root.checked) return Theme.accent
                if (root.variant === "danger") return "#FCA5A5"
                if (root.variant === "accent") return Theme.accent
                return Theme.textPrimary
            }
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            visible: root.text !== ""
            text: root.text
            font.family: Theme.fontBody
            font.pixelSize: root.compact ? 11 : 12
            font.weight: root.variant === "primary" ? Font.DemiBold : Font.Normal
            color: {
                if (root.variant === "primary") return "#090A0D"
                if (root.checked) return Theme.accent
                if (root.variant === "danger") return "#FCA5A5"
                if (root.variant === "accent") return Theme.accent
                return Theme.textPrimary
            }
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        enabled: root.enabled

        onClicked: {
            if (root.checkable) {
                root.checked = !root.checked
            }
            root.clicked()
        }
    }
}
