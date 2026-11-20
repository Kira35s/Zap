import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Says what went wrong (a source that failed to load, a guide that's broken)
// instead of quietly showing less.
Rectangle {
    id: banner
    visible: backend.problems.length > 0
    height: layout.implicitHeight + 24
    color: "#2a1719"
    border.color: "#6b2a2f"
    radius: theme.radius

    RowLayout {
        id: layout
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        Label {
            text: backend.problems.join("\n")
            color: "#ffd0d0"
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
        Button {
            Material.roundedScale: theme.radius
            text: qsTr("Dismiss")
            flat: true
            onClicked: backend.dismissProblems()
        }
    }
}
