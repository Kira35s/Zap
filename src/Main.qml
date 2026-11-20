import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window
import QtMultimedia

ApplicationWindow {
    id: win
    width: 1360
    height: 820
    minimumWidth: 960
    minimumHeight: 580
    visible: true
    title: playing.name ? playing.name + " — Zap" : "Zap"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    readonly property color accent: theme.accent
    readonly property color ink: "#e8e8ec"
    readonly property color muted: "#8b8b96"
    readonly property color panel: "#141417"
    readonly property color raised: "#1c1c21"
    readonly property int small: Math.min(4, theme.radius)

    property string selectedKey: ""
    property string playingKey: ""
    property var playing: ({})
    property bool expanded: false
    property bool helpVisible: false
    property real pendingSeek: 0
    readonly property bool isLive: playing.kind === "live"

    function tint(alpha) { return Qt.rgba(accent.r, accent.g, accent.b, alpha) }

    // Bindings that format times read the language, so they re-render on a switch.
    function clock(seconds) {
        translator.language
        return new Date(seconds * 1000).toLocaleTimeString(Qt.locale(), Locale.ShortFormat)
    }
    function day(seconds) {
        translator.language
        return new Date(seconds * 1000).toLocaleDateString(Qt.locale(), "dddd d MMMM")
    }
    function duration(ms) {
        const s = Math.max(0, Math.floor(ms / 1000))
        const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60), x = s % 60
        return (h > 0 ? h + ":" + String(m).padStart(2, "0") : m) + ":" + String(x).padStart(2, "0")
    }
    function sectionTitle(id) {
        switch (id) {
        case "live": return qsTr("Live TV")
        case "guide": return qsTr("TV guide")
        case "movies": return qsTr("Movies")
        case "series": return qsTr("Series")
        case "favorites": return qsTr("Favourites")
        case "continue": return qsTr("Continue watching")
        case "search": return qsTr("Search")
        case "sources": return qsTr("Sources")
        case "settings": return qsTr("Settings")
        }
        return ""
    }

    // Playback

    function play(key, expand) {
        const e = backend.entry(key)
        if (!e.url)
            return
        saveProgress()
        playing = e
        playingKey = key
        pendingSeek = e.kind === "live" ? 0 : (e.position || 0)
        player.source = e.url
        player.play()
        backend.played(key)
        if (expand)
            setExpanded(true)
    }
    function activate(key) {
        if (!key)
            return
        const e = backend.entry(key)
        if (e.kind === "series") {
            backend.openSeries(key)
            return
        }
        if (e.kind === "live") {
            // The first Enter previews the channel, the second fills the screen.
            if (playingKey === key && player.playbackState !== MediaPlayer.StoppedState)
                setExpanded(true)
            else
                play(key, false)
            return
        }
        play(key, true)
    }
    function stop() {
        saveProgress()
        player.stop()
        player.source = ""
        playingKey = ""
        playing = ({})
        setExpanded(false)
    }
    function saveProgress() {
        if (playingKey && playing.kind !== "live" && player.duration > 0)
            backend.savePosition(playingKey, player.position, player.duration)
    }
    function applyPendingSeek() {
        if (pendingSeek > 0 && player.seekable
                && (player.mediaStatus === MediaPlayer.LoadedMedia || player.mediaStatus === MediaPlayer.BufferedMedia
                    || player.mediaStatus === MediaPlayer.BufferingMedia)) {
            player.setPosition(pendingSeek)
            pendingSeek = 0
        }
    }
    function seek(deltaMs) {
        if (!player.seekable)
            return
        player.setPosition(Math.max(0, Math.min(player.duration, player.position + deltaMs)))
        playerView.showOsd()
    }
    function zap(delta) {
        if (!isLive)
            return
        const key = backend.neighbor(playingKey, delta)
        if (key && key !== playingKey) {
            play(key, false)
            playerView.showOsd()
        }
    }
    function setExpanded(on) {
        if (on === expanded)
            return
        expanded = on
        if (on) {
            win.showFullScreen()
            playerView.showOsd()
        } else {
            win.showNormal()
            Qt.callLater(restoreFocus)
        }
    }

    // Navigation

    function goSection(id) {
        backend.section = id
        Qt.callLater(restoreFocus)
    }
    function restoreFocus() {
        if (contentLoader.item)
            contentLoader.item.forceActiveFocus()
    }
    function focusCategories() {
        if (categoryList.visible)
            categoryList.focusList()
    }
    function startSearch() {
        setExpanded(false)
        backend.section = "search"
        Qt.callLater(() => { if (contentLoader.item && contentLoader.item.focusSearch) contentLoader.item.focusSearch() })
    }
    function back() {
        if (helpVisible)
            helpVisible = false
        else if (expanded)
            isLive ? setExpanded(false) : stop()
        else if (contentLoader.item && contentLoader.item.back && contentLoader.item.back())
            return
        else if (backend.seriesOpen) {
            backend.closeSeries()
            Qt.callLater(restoreFocus)
        } else if (backend.section === "search" && backend.query)
            backend.query = ""
        else
            restoreFocus()
    }

    MediaPlayer {
        id: player
        audioOutput: AudioOutput {
            id: audio
            volume: backend.volume
        }
        videoOutput: playerView.output
        onMediaStatusChanged: {
            win.applyPendingSeek()
            // Series play on: the next episode starts when one ends.
            if (mediaStatus === MediaPlayer.EndOfMedia && win.playingKey) {
                win.saveProgress()
                const next = backend.nextEpisode(win.playingKey)
                if (next)
                    win.play(next, true)
                else if (!win.isLive)
                    win.stop()
            }
        }
        onSeekableChanged: win.applyPendingSeek()
    }

    Timer {
        interval: 10000
        repeat: true
        running: win.playingKey !== "" && !win.isLive && player.playbackState === MediaPlayer.PlayingState
        onTriggered: win.saveProgress()
    }

    onClosing: saveProgress()

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: { win.saveProgress(); Qt.quit() } }
    Shortcut { sequence: "Esc"; context: Qt.ApplicationShortcut; onActivated: win.back() }
    Shortcut {
        sequence: "Space"
        context: Qt.ApplicationShortcut
        enabled: win.playingKey !== ""
        onActivated: {
            player.playbackState === MediaPlayer.PlayingState ? player.pause() : player.play()
            playerView.showOsd()
        }
    }
    Shortcut {
        sequence: "F11"
        context: Qt.ApplicationShortcut
        onActivated: win.visibility === Window.FullScreen ? win.showNormal() : win.showFullScreen()
    }
    Shortcut { sequences: ["/", "Ctrl+F"]; context: Qt.ApplicationShortcut; onActivated: win.startSearch() }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; enabled: !win.expanded; onActivated: win.goSection("live") }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; enabled: !win.expanded; onActivated: win.goSection("guide") }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; enabled: !win.expanded; onActivated: win.goSection("movies") }
    Shortcut { sequence: "4"; context: Qt.ApplicationShortcut; enabled: !win.expanded; onActivated: win.goSection("series") }
    Shortcut { sequence: "5"; context: Qt.ApplicationShortcut; enabled: !win.expanded; onActivated: win.goSection("favorites") }
    Shortcut { sequence: "6"; context: Qt.ApplicationShortcut; enabled: !win.expanded; onActivated: win.goSection("continue") }
    Shortcut { sequence: "7"; context: Qt.ApplicationShortcut; enabled: !win.expanded; onActivated: win.goSection("sources") }
    Shortcut { sequence: "8"; context: Qt.ApplicationShortcut; enabled: !win.expanded; onActivated: win.goSection("settings") }
    Shortcut {
        sequence: "F"
        context: Qt.ApplicationShortcut
        onActivated: {
            const key = win.expanded ? win.playingKey : (backend.seriesOpen && !win.selectedKey ? backend.series.key : win.selectedKey)
            if (key) {
                backend.toggleFavorite(key)
                playerView.showOsd()
            }
        }
    }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; enabled: !win.expanded; onActivated: backend.refresh() }
    Shortcut {
        sequence: "O"
        context: Qt.ApplicationShortcut
        onActivated: {
            const key = win.expanded || !win.selectedKey ? win.playingKey : win.selectedKey
            const position = key === win.playingKey ? player.position : backend.resumePosition(key)
            if (key && backend.openExternally(key, position) && win.playingKey) {
                win.saveProgress()
                player.pause()
            }
        }
    }
    Shortcut { sequence: "M"; context: Qt.ApplicationShortcut; onActivated: { audio.muted = !audio.muted; playerView.showOsd() } }
    Shortcut { sequence: "X"; context: Qt.ApplicationShortcut; enabled: win.playingKey !== ""; onActivated: win.stop() }
    Shortcut { sequences: ["+", "="]; context: Qt.ApplicationShortcut; onActivated: { backend.volume = backend.volume + 0.1; playerView.showOsd() } }
    Shortcut { sequence: "-"; context: Qt.ApplicationShortcut; onActivated: { backend.volume = backend.volume - 0.1; playerView.showOsd() } }
    Shortcut { sequences: ["Up", "PgUp"]; context: Qt.ApplicationShortcut; enabled: win.expanded && win.isLive; onActivated: win.zap(-1) }
    Shortcut { sequences: ["Down", "PgDown"]; context: Qt.ApplicationShortcut; enabled: win.expanded && win.isLive; onActivated: win.zap(1) }
    Shortcut { sequence: "PgUp"; context: Qt.ApplicationShortcut; enabled: !win.expanded && win.isLive && backend.section !== "guide"; onActivated: win.zap(-1) }
    Shortcut { sequence: "PgDown"; context: Qt.ApplicationShortcut; enabled: !win.expanded && win.isLive && backend.section !== "guide"; onActivated: win.zap(1) }
    Shortcut { sequence: "Left"; context: Qt.ApplicationShortcut; enabled: win.expanded && !win.isLive; onActivated: win.seek(-10000) }
    Shortcut { sequence: "Right"; context: Qt.ApplicationShortcut; enabled: win.expanded && !win.isLive; onActivated: win.seek(10000) }
    Shortcut { sequence: "Shift+Left"; context: Qt.ApplicationShortcut; enabled: win.expanded && !win.isLive; onActivated: win.seek(-60000) }
    Shortcut { sequence: "Shift+Right"; context: Qt.ApplicationShortcut; enabled: win.expanded && !win.isLive; onActivated: win.seek(60000) }
    Shortcut { sequences: ["Return", "Enter", "I"]; context: Qt.ApplicationShortcut; enabled: win.expanded; onActivated: playerView.showOsd() }
    Shortcut {
        sequence: "Del"
        context: Qt.ApplicationShortcut
        enabled: !win.expanded && backend.section === "continue" && win.selectedKey !== ""
        onActivated: backend.forget(win.selectedKey)
    }

    Component.onCompleted: {
        if (backend.sources.length === 0)
            backend.section = "sources"
        Qt.callLater(restoreFocus)
    }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        id: root
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        RowLayout {
            anchors.fill: parent
            spacing: 0
            visible: !win.expanded

            NavRail {
                Layout.fillHeight: true
                Layout.preferredWidth: 210
            }

            CategoryList {
                id: categoryList
                Layout.fillHeight: true
                Layout.preferredWidth: 260
                visible: backend.categories.length > 0 && !backend.seriesOpen
            }

            Loader {
                id: contentLoader
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 0
                Layout.preferredHeight: 0
                sourceComponent: backend.section === "settings" ? settingsPage
                               : backend.section === "sources" ? sourcesPage
                               : backend.seriesOpen ? seriesPage
                               : backend.section === "guide" ? guidePage
                               : backend.section === "movies" || backend.section === "series" ? gridPage
                               : listPage
                onLoaded: Qt.callLater(win.restoreFocus)
            }

            InfoPane {
                id: infoPane
                Layout.fillHeight: true
                Layout.preferredWidth: Math.round(root.width * 0.3)
                Layout.preferredHeight: 0
                visible: backend.section !== "guide" && backend.section !== "sources" && backend.section !== "settings"
            }
        }

        Rectangle {
            id: expandedHost
            anchors.fill: parent
            color: "black"
            visible: win.expanded
        }

        PlayerView {
            id: playerView
            parent: win.expanded ? expandedHost
                  : backend.section === "guide" && contentLoader.item && contentLoader.item.videoHost
                    ? contentLoader.item.videoHost : infoPane.videoHost
            anchors.fill: parent
            visible: win.playingKey !== ""
        }

        ProblemBanner {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 16
            width: Math.min(parent.width - 32, 720)
        }

        HelpOverlay {
            anchors.fill: parent
            visible: win.helpVisible
        }
    }

    Component { id: listPage; EntryPage {} }
    Component { id: gridPage; PosterGrid {} }
    Component { id: guidePage; GuideGrid {} }
    Component { id: seriesPage; SeriesPage {} }
    Component { id: sourcesPage; SourcesPage {} }
    Component { id: settingsPage; SettingsPage {} }
}
