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

        ToolButton {
            id: playPauseButton
            Layout.preferredWidth: 38
            Layout.preferredHeight: 38
            enabled: Player.hasAudio
            text: Player.isPlaying ? "Ⅱ" : "▶"
            font.pixelSize: 17
            Accessible.name: Player.isPlaying ? "Pause" : "Play"
            ToolTip.visible: hovered
            ToolTip.text: Player.isPlaying ? "Pause" : "Play"
            onClicked: Player.isPlaying ? Player.pause() : Player.play()
        }

        ToolButton {
            Layout.preferredWidth: 38
            Layout.preferredHeight: 38
            enabled: Player.hasAudio
            text: "■"
            font.pixelSize: 14
            Accessible.name: "Stop"
            ToolTip.visible: hovered
            ToolTip.text: "Stop"
            onClicked: Player.stop()
        }
    }
}
