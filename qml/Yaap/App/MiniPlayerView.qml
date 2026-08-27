import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Yaap.App 1.0
import Yaap.ModApi 1.0

Item {
    id: root

    required property var hostWindow

    readonly property string displayTitle: Player.hasNowPlayingMetadata
        ? (Player.nowPlayingTitle.length > 0
            ? Player.nowPlayingTitle : Player.nowPlayingText)
        : Player.title
    readonly property string displayContext: {
        const values = [Player.nowPlayingArtist, Player.stationTitle]
            .filter(value => value.length > 0)
        if (Player.nowPlayingMetadataStale)
            values.push("reconnecting")
        return values.join(" · ")
    }

    function focusTransport() { playPauseButton.forceActiveFocus() }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.ArrowCursor
        onPressed: root.hostWindow.startSystemMove()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.miniControlAreaLeftInset
        anchors.topMargin: Theme.miniControlAreaTopInset
        anchors.rightMargin: Theme.miniControlAreaRightInset
        anchors.bottomMargin: Theme.miniControlAreaBottomInset
        spacing: Math.min(Theme.spacing, 10)

        Label {
            text: "YAAP"
            color: Theme.accent
            font.pixelSize: 12
            font.bold: true
            font.letterSpacing: 2
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Label {
                id: miniTitle
                Layout.fillWidth: true
                text: root.displayTitle
                color: Theme.primaryText
                font.pixelSize: 16
                font.weight: Font.DemiBold
                elide: Text.ElideRight

                HoverHandler { id: miniTitleHover }
                ToolTip {
                    visible: miniTitleHover.hovered && miniTitle.truncated
                    text: miniTitle.text
                    delay: 350
                }
            }

            Label {
                id: miniContext
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.displayContext
                color: Theme.secondaryText
                font.pixelSize: 11
                elide: Text.ElideRight

                HoverHandler { id: miniContextHover }
                ToolTip {
                    visible: miniContextHover.hovered && miniContext.truncated
                    text: miniContext.text
                    delay: 350
                }
            }

            Slider {
                id: miniSeek
                Layout.fillWidth: true
                Layout.preferredHeight: 20
                visible: Player.durationMilliseconds > 0
                from: 0
                to: Math.max(1, Player.durationMilliseconds)
                enabled: Player.hasAudio
                Accessible.name: "Seek"
                onPressedChanged: if (!pressed) Player.seek(Math.round(value))
                Binding on value {
                    when: !miniSeek.pressed
                    value: Player.positionMilliseconds
                }
            }
        }

        TransportButton {
            id: playPauseButton
            Layout.preferredWidth: 38
            Layout.preferredHeight: 38
            enabled: Player.hasAudio
            iconSource: Player.isPlaying
                ? Qt.resolvedUrl("icons/pause.svg")
                : Qt.resolvedUrl("icons/play.svg")
            accessibleLabel: Player.isPlaying ? "Pause" : "Play"
            onClicked: Player.isPlaying ? Player.pause() : Player.play()
        }

        TransportButton {
            Layout.preferredWidth: 38
            Layout.preferredHeight: 38
            enabled: Player.hasAudio
            iconSource: Qt.resolvedUrl("icons/stop.svg")
            accessibleLabel: "Stop"
            onClicked: Player.stop()
        }

        TransportButton {
            Layout.preferredWidth: 34
            Layout.preferredHeight: 34
            iconSource: Player.muted || Player.volume <= 0
                ? Qt.resolvedUrl("icons/mute.svg")
                : Qt.resolvedUrl("icons/volume.svg")
            accessibleLabel: Player.muted ? "Unmute" : "Mute"
            onClicked: Player.toggleMuted()
        }

        Slider {
            Layout.preferredWidth: 68
            from: 0
            to: 1
            stepSize: 0.01
            value: Player.volume
            Accessible.name: "Volume"
            onMoved: Player.volume = value
        }
    }
}
