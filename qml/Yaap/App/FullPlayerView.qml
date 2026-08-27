import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Yaap.App 1.0
import Yaap.ModApi 1.0
import Yaap.Ui 1.0

Item {
    id: root

    required property var hostWindow

    signal providersRequested()
    signal accountsRequested()
    signal libraryRequested()
    signal radioRequested()
    signal modsRequested()
    signal fileRequested()
    signal streamRequested()
    signal folderRequested()

    function formatTime(milliseconds) {
        const totalSeconds = Math.max(0, Math.floor(milliseconds / 1000))
        const minutes = Math.floor(totalSeconds / 60)
        const seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    function focusTransport() { playPauseButton.forceActiveFocus() }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        RowLayout {
            Layout.fillWidth: true
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                Layout.minimumWidth: 72

                Label {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: "YAAP"
                    color: Theme.accent
                    font.pixelSize: 15
                    font.bold: true
                    font.letterSpacing: 3
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.ArrowCursor
                    onPressed: root.hostWindow.startSystemMove()
                }
            }
            Button {
                text: "Providers (" + Providers.availableProviderCount + ")"
                onClicked: root.providersRequested()
            }
            Button {
                text: "Accounts (" + ProviderAccounts.count + ")"
                onClicked: root.accountsRequested()
            }
            Button { text: "Library"; onClicked: root.libraryRequested() }
            Button { text: "Radio (" + Radio.count + ")"; onClicked: root.radioRequested() }
            Button { text: "Mods"; onClicked: root.modsRequested() }
            Label { text: Player.stateName; color: Theme.secondaryText }
        }

        ExtensionHost {
            slotId: "navigation.primary"
            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true }

        Label {
            id: nowPlayingTitle
            Layout.fillWidth: true
            text: Player.hasNowPlayingMetadata
                ? (Player.nowPlayingTitle.length > 0
                    ? Player.nowPlayingTitle : Player.nowPlayingText)
                : Player.title
            color: Theme.primaryText
            font.pixelSize: 30
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideMiddle

            HoverHandler { id: nowPlayingTitleHover }
            ToolTip {
                visible: nowPlayingTitleHover.hovered && nowPlayingTitle.truncated
                text: nowPlayingTitle.text
                delay: 400
            }
        }

        Label {
            Layout.fillWidth: true
            visible: Player.hasNowPlayingMetadata
            text: [Player.nowPlayingArtist, Player.stationTitle]
                .filter(value => value.length > 0).join(" · ")
                + (Player.nowPlayingMetadataStale ? " · reconnecting" : "")
            color: Theme.secondaryText
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideMiddle
        }

        ExtensionHost {
            slotId: "nowPlaying.aboveTransport"
            Layout.fillWidth: true
        }

        Label {
            Layout.fillWidth: true
            visible: Player.errorMessage.length > 0
            text: Player.errorMessage
            color: Theme.error
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: Player.isLoading || Player.isBuffering
            visible: running
        }

        Slider {
            id: seekSlider
            Layout.fillWidth: true
            from: 0
            to: Math.max(1, Player.durationMilliseconds)
            enabled: Player.hasAudio && Player.durationMilliseconds > 0
            onPressedChanged: if (!pressed) Player.seek(Math.round(value))
            Binding on value {
                when: !seekSlider.pressed
                value: Player.positionMilliseconds
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label { text: root.formatTime(Player.positionMilliseconds); color: Theme.secondaryText }
            Item { Layout.fillWidth: true }
            Label { text: root.formatTime(Player.durationMilliseconds); color: Theme.secondaryText }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.spacing
            Button { text: "Open file"; enabled: !Player.isLoading; onClicked: root.fileRequested() }
            Button { text: "Open stream"; enabled: !Player.isLoading; onClicked: root.streamRequested() }
            Button {
                text: MusicLibrary.scanning ? "Scanning…" : "Add library (" + MusicLibrary.trackCount + ")"
                enabled: !MusicLibrary.scanning
                onClicked: root.folderRequested()
            }
            Button {
                id: playPauseButton
                text: Player.isPlaying ? "Pause" : "Play"
                enabled: Player.hasAudio
                onClicked: Player.isPlaying ? Player.pause() : Player.play()
            }
            Button { text: "Stop"; enabled: Player.hasAudio; onClicked: Player.stop() }
        }

        ExtensionHost {
            slotId: "nowPlaying.toolbar.after"
            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true }
        Label {
            Layout.fillWidth: true
            text: "Extension API " + ModApi.version
                + " · FFmpeg → bounded PCM stream → miniaudio"
            color: Theme.secondaryText
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
