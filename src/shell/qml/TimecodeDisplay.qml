import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    property double seconds: 0.0
    property double totalSeconds: 0.0
    property double fps: 30.0

    radius: Theme.radiusSmall
    color: Theme.bgApp
    border.color: Theme.borderMedium
    border.width: 1

    implicitWidth: tcRow.implicitWidth + 16
    implicitHeight: 28

    Row {
        id: tcRow
        anchors.centerIn: parent
        spacing: 6

        Text {
            text: formatTimecode(root.seconds, root.fps)
            font.family: Theme.fontMono
            font.pixelSize: 13
            font.weight: Font.Bold
            color: Theme.cyan
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            visible: root.totalSeconds > 0
            text: "/"
            font.family: Theme.fontMono
            font.pixelSize: 11
            color: Theme.textTertiary
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            visible: root.totalSeconds > 0
            text: formatTimecode(root.totalSeconds, root.fps)
            font.family: Theme.fontMono
            font.pixelSize: 11
            color: Theme.textSecondary
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    function formatTimecode(sec, rate) {
        var s = Math.max(0, sec)
        var totalFrames = Math.floor(s * rate)
        var frames = totalFrames % Math.floor(rate)
        var totalSec = Math.floor(s)
        var secondsPart = totalSec % 60
        var totalMin = Math.floor(totalSec / 60)
        var minutesPart = totalMin % 60
        var hoursPart = Math.floor(totalMin / 60)

        function pad(n, width) {
            var z = n.toString()
            while (z.length < width) z = "0" + z
            return z
        }

        return pad(hoursPart, 2) + ":" + pad(minutesPart, 2) + ":" + pad(secondsPart, 2) + ":" + pad(frames, 2)
    }
}
