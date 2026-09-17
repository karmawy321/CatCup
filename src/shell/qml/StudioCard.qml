import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property string title: ""
    property string iconText: ""
    property bool collapsible: false
    property bool collapsed: false

    default property alias content: contentLayout.data

    radius: Theme.radiusMedium
    color: Theme.bgCard
    border.color: Theme.borderMedium
    border.width: 1

    implicitWidth: 260
    implicitHeight: headerRow.implicitHeight + (collapsed ? 0 : contentContainer.implicitHeight) + 12

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // Header
        Item {
            id: headerContainer
            Layout.fillWidth: true
            implicitHeight: headerRow.implicitHeight

            RowLayout {
                id: headerRow
                anchors.fill: parent
                spacing: 6

                Text {
                    visible: root.iconText !== ""
                    text: root.iconText
                    font.pixelSize: 13
                    color: Theme.accent
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    text: root.title
                    font.family: Theme.fontBody
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    visible: root.collapsible
                    text: root.collapsed ? "▶" : "▼"
                    font.pixelSize: 10
                    color: Theme.textTertiary
                    Layout.alignment: Qt.AlignVCenter
                }
            }

            MouseArea {
                visible: root.collapsible
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.collapsed = !root.collapsed
            }
        }

        // Content Area
        Item {
            id: contentContainer
            Layout.fillWidth: true
            visible: !root.collapsed
            implicitHeight: contentLayout.implicitHeight

            ColumnLayout {
                id: contentLayout
                anchors.fill: parent
                spacing: 8
            }
        }
    }
}
