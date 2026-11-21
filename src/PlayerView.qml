import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtMultimedia

Rectangle {
    id: view
    color: "black"
    radius: win.expanded ? 0 : theme.radius
    clip: true

    property alias output: video
    property bool osd: false
    readonly property var live: win.isLive ? (backend.revision, backend.entry(win.playingKey)) : ({})
    readonly property bool waiting: player.error === MediaPlayer.NoError
        && (player.mediaStatus === MediaPlayer.LoadingMedia || player.mediaStatus === MediaPlayer.StalledMedia)

    property string noticeText: ""
    readonly property bool menuOpen: audioMenu.opened || subtitleMenu.opened

    function showOsd() {
        osd = true
        osdTimer.restart()
    }
    // A short message over the picture, like the track just picked.
    function notice(text) {
        noticeText = text
        noticeTimer.restart()
        showOsd()
    }

    Timer {
        id: osdTimer
        interval: 4000
        onTriggered: view.menuOpen ? restart() : (view.osd = false)
    }
    Timer {
        id: noticeTimer
        interval: 2500
        onTriggered: view.noticeText = ""
    }

    VideoOutput {
        id: video
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
    }

    // Under the OSD, so its buttons and slider get their own clicks.
    MouseArea {
        anchors.fill: parent
        hoverEnabled: win.expanded
        cursorShape: win.expanded && !view.osd ? Qt.BlankCursor : Qt.ArrowCursor
        onPositionChanged: if (win.expanded) view.showOsd()
        onClicked: win.expanded ? view.showOsd() : win.setExpanded(true)
        onDoubleClicked: if (win.expanded) win.setExpanded(false)
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: view.waiting
        visible: view.waiting
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 32, 560)
        visible: player.error !== MediaPlayer.NoError
        spacing: 6
        Label {
            text: qsTr("This stream can't be played.")
            color: win.ink
            font.pixelSize: win.expanded ? 22 : 15
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        Label {
            text: player.errorString
            color: win.muted
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }
        Label {
            text: qsTr("Press O to try it in mpv.")
            color: win.muted
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
    }

    Label {
        anchors.centerIn: parent
        visible: player.playbackState === MediaPlayer.PausedState
        text: "❚❚"
        color: "white"
        font.pixelSize: win.expanded ? 56 : 32
        opacity: 0.85
    }

    Label {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: win.expanded ? 28 : 10
        visible: audio.muted || (view.osd && win.expanded)
        text: audio.muted ? qsTr("Muted") : qsTr("Volume %1%").arg(Math.round(backend.volume * 100))
        color: "white"
        padding: 6
        background: Rectangle { color: "#aa000000"; radius: win.small }
    }

    Label {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: win.expanded ? 28 : 10
        visible: view.noticeText !== ""
        text: view.noticeText
        color: "white"
        font.pixelSize: win.expanded ? 18 : 13
        padding: 8
        background: Rectangle { color: "#cc000000"; radius: win.small }
    }

    // What's on, shown for a few seconds after anything changes.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: osdColumn.implicitHeight + 72
        visible: win.expanded && (view.osd || view.menuOpen || player.playbackState !== MediaPlayer.PlayingState)
        gradient: Gradient {
            GradientStop { position: 0; color: "transparent" }
            GradientStop { position: 0.35; color: "#cc000000" }
            GradientStop { position: 1; color: "#f0000000" }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
            onPositionChanged: view.showOsd()
        }

        ColumnLayout {
            id: osdColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 36
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 14
                Image {
                    source: win.playing.logo || ""
                    visible: win.isLive && status === Image.Ready
                    fillMode: Image.PreserveAspectFit
                    sourceSize.height: 96
                    Layout.preferredWidth: 80
                    Layout.preferredHeight: 48
                }
                Label {
                    text: (win.isLive && win.playing.number > 0 ? win.playing.number + "  " : "")
                          + (win.playing.kind === "episode" && win.playing.seriesName
                             ? win.playing.seriesName + " — " : "")
                          + (win.playing.name || "")
                          + ((backend.revision, backend.entry(win.playingKey).favorite) ? "  ★" : "")
                    color: "white"
                    font.pixelSize: 26
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Label {
                    text: win.clock(backend.now)
                    color: "#d0d0d6"
                    font.pixelSize: 20
                }
            }

            // Live: what's on now and next.
            Label {
                visible: win.isLive
                text: view.live.nowTitle
                      ? win.clock(view.live.nowStart) + " – " + win.clock(view.live.nowStop) + "   " + view.live.nowTitle
                      : qsTr("No programme information")
                color: "white"
                font.pixelSize: 18
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            ProgressBar {
                visible: win.isLive && !!view.live.nowTitle
                from: 0
                to: 1
                value: view.live.nowStop > view.live.nowStart
                       ? (backend.now - view.live.nowStart) / (view.live.nowStop - view.live.nowStart) : 0
                Layout.fillWidth: true
            }
            Label {
                visible: win.isLive && !!view.live.nextTitle
                text: qsTr("Next at %1: %2").arg(win.clock(view.live.nextStart || 0)).arg(view.live.nextTitle || "")
                color: "#b9b9c2"
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            // Movies and episodes: where we are.
            RowLayout {
                visible: !win.isLive
                Layout.fillWidth: true
                spacing: 14
                Label {
                    text: win.duration(player.position)
                    color: "white"
                }
                Slider {
                    from: 0
                    to: Math.max(1, player.duration)
                    value: player.position
                    enabled: player.seekable
                    Layout.fillWidth: true
                    onMoved: player.setPosition(value)
                }
                Label {
                    text: win.duration(player.duration)
                    color: "white"
                }
            }

            // Language and subtitles, when the stream has a choice.
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: player.audioTracks.length > 1 || player.subtitleTracks.length > 0

                Button {
                    id: audioButton
                    visible: player.audioTracks.length > 1
                    focusPolicy: Qt.NoFocus
                    flat: true
                    Material.foreground: "white"
                    Material.roundedScale: theme.radius
                    text: {
                        win.trackRevision
                        const track = win.tracks("audio")[Math.max(0, player.activeAudioTrack)]
                        return qsTr("Audio: %1").arg(track ? track.label : "—") + "  ▾"
                    }
                    onClicked: audioMenu.open()
                    Menu {
                        id: audioMenu
                        y: -height
                        Repeater {
                            model: audioMenu.opened ? win.tracks("audio") : []
                            delegate: MenuItem {
                                required property var modelData
                                required property int index
                                text: modelData.label
                                checkable: true
                                checked: (win.trackRevision, index === Math.max(0, player.activeAudioTrack))
                                onTriggered: win.chooseAudio(index)
                            }
                        }
                    }
                }
                Button {
                    id: subtitleButton
                    visible: player.subtitleTracks.length > 0
                    focusPolicy: Qt.NoFocus
                    flat: true
                    Material.foreground: "white"
                    Material.roundedScale: theme.radius
                    text: {
                        win.trackRevision
                        const track = win.tracks("subtitle")[player.activeSubtitleTrack]
                        return (track ? qsTr("Subtitles: %1").arg(track.label) : qsTr("Subtitles off")) + "  ▾"
                    }
                    onClicked: subtitleMenu.open()
                    Menu {
                        id: subtitleMenu
                        y: -height
                        MenuItem {
                            text: qsTr("Off")
                            checkable: true
                            checked: (win.trackRevision, player.activeSubtitleTrack < 0)
                            onTriggered: win.chooseSubtitle(-1)
                        }
                        Repeater {
                            model: subtitleMenu.opened ? win.tracks("subtitle") : []
                            delegate: MenuItem {
                                required property var modelData
                                required property int index
                                text: modelData.label
                                checkable: true
                                checked: (win.trackRevision, index === player.activeSubtitleTrack)
                                onTriggered: win.chooseSubtitle(index)
                            }
                        }
                    }
                }
                Item { Layout.fillWidth: true }
            }

            Label {
                text: win.isLive
                      ? qsTr("↑ ↓ change channel · Space pause · A audio · S subtitles · F favourite · Esc back to the list · ? shortcuts")
                      : qsTr("← → 10 seconds · Shift+← → 1 minute · Space pause · A audio · S subtitles · Esc stop · ? shortcuts")
                color: "#8b8b96"
                font.pixelSize: 13
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
        }
    }

}
