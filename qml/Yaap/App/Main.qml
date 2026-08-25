import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import Yaap.App 1.0
import Yaap.ModApi 1.0
import Yaap.Ui 1.0

ApplicationWindow {
    id: root

    width: 900
    height: 560
    minimumWidth: 680
    minimumHeight: 420
    visible: true
    title: "Yaap — Music player prototype"
    color: Theme.windowBottom
    palette.window: Theme.surface
    palette.windowText: Theme.primaryText
    palette.base: Theme.windowBottom
    palette.alternateBase: Theme.surface
    palette.text: Theme.primaryText
    palette.button: Theme.surface
    palette.buttonText: Theme.primaryText
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.windowBottom
    palette.placeholderText: Theme.secondaryText

    function formatTime(milliseconds) {
        const totalSeconds = Math.max(0, Math.floor(milliseconds / 1000))
        const minutes = Math.floor(totalSeconds / 60)
        const seconds = totalSeconds % 60
        return minutes + ":" + (seconds < 10 ? "0" : "") + seconds
    }

    FileDialog {
        id: fileDialog
        title: "Open an audio file"
        fileMode: FileDialog.OpenFile
        nameFilters: [
            "Audio files (*.mp3 *.flac *.wav *.m4a *.aac *.ogg *.opus)",
            "All files (*)"
        ]
        onAccepted: Player.openFile(selectedFile)
    }

    FolderDialog {
        id: folderDialog
        title: "Add music library folder"
        onAccepted: MusicLibrary.addFolder(selectedFolder)
    }

    Dialog {
        id: streamDialog
        title: "Open internet radio or provider stream"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: playlistMode.checked
            ? Player.openRadioPlaylist(streamUrl.text)
            : Player.openStream(streamUrl.text, streamName.text)

        ColumnLayout {
            anchors.fill: parent
            Label { text: "HTTP(S) stream URL" }
            TextField {
                id: streamUrl
                Layout.preferredWidth: 440
                placeholderText: "https://radio.example/live"
            }
            Label { text: "Display name (optional)" }
            TextField { id: streamName; Layout.fillWidth: true }
            CheckBox {
                id: playlistMode
                text: "URL is an M3U, PLS, or XSPF station playlist"
            }
        }
    }

    Dialog {
        id: trustDialog
        property string pendingModId
        property var requestedPermissions: []
        property bool activateThemeAfterGrant: false

        title: "Trust third-party mod?"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: {
            if (Mods.grantDeclared(pendingModId)) {
                if (activateThemeAfterGrant)
                    Mods.activateTheme(pendingModId)
                else
                    Mods.setEnabled(pendingModId, true)
            }
            activateThemeAfterGrant = false
        }
        onRejected: activateThemeAfterGrant = false

        ColumnLayout {
            width: 500
            Label {
                Layout.fillWidth: true
                text: ModApi.trustWarning
                color: Theme.error
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                text: "Requested permissions:\n" + trustDialog.requestedPermissions.join("\n")
                color: Theme.primaryText
                wrapMode: Text.Wrap
            }
        }
    }

    Dialog {
        id: modsDialog
        title: "Mods and themes — API " + ModApi.version
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Close
        width: 680
        height: 460

        contentItem: ColumnLayout {
            spacing: Theme.spacing

            Label {
                Layout.fillWidth: true
                visible: Mods.errorMessage.length > 0
                text: Mods.errorMessage
                color: Theme.error
                wrapMode: Text.Wrap
            }

            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: Theme.spacing
                model: Mods

                delegate: ThemedPanel {
                    required property string modId
                    required property string name
                    required property string version
                    required property var kinds
                    required property var permissions
                    required property bool modEnabled
                    required property bool permissionsGranted
                    required property string diagnostic
                    required property bool isTheme
                    required property bool isUiExtension

                    width: ListView.view.width
                    height: details.implicitHeight + 24

                    ColumnLayout {
                        id: details
                        anchors.fill: parent
                        anchors.margins: 12

                        Label {
                            text: name + "  " + version
                            color: Theme.primaryText
                            font.bold: true
                        }
                        Label {
                            Layout.fillWidth: true
                            text: diagnostic.length > 0
                                ? diagnostic
                                : kinds.join(", ") + (isUiExtension ? " — trusted in-process code" : "")
                            color: diagnostic.length > 0 ? Theme.error : Theme.secondaryText
                            wrapMode: Text.Wrap
                        }
                        RowLayout {
                            Button {
                                text: modEnabled ? "Disable" : permissionsGranted ? "Enable" : "Review and grant"
                                enabled: diagnostic.length === 0
                                onClicked: {
                                    if (modEnabled) {
                                        Mods.setEnabled(modId, false)
                                    } else if (permissionsGranted) {
                                        Mods.setEnabled(modId, true)
                                    } else {
                                        trustDialog.pendingModId = modId
                                        trustDialog.requestedPermissions = permissions
                                        trustDialog.activateThemeAfterGrant = false
                                        trustDialog.open()
                                    }
                                }
                            }
                            Button {
                                visible: isTheme
                                text: Theme.currentThemeId === modId ? "Active theme" : "Use theme"
                                enabled: diagnostic.length === 0 && Theme.currentThemeId !== modId
                                onClicked: {
                                    if (!permissionsGranted) {
                                        trustDialog.pendingModId = modId
                                        trustDialog.requestedPermissions = permissions
                                        trustDialog.activateThemeAfterGrant = true
                                        trustDialog.open()
                                    } else {
                                        Mods.activateTheme(modId)
                                    }
                                }
                            }
                            Button {
                                text: "Revoke"
                                visible: permissionsGranted
                                onClicked: Mods.revokeAll(modId)
                            }
                        }
                    }
                }
            }

            Button { text: "Rescan packages"; onClicked: Mods.refresh() }
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.windowTop }
            GradientStop { position: 1.0; color: Theme.windowBottom }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 36
        spacing: Theme.spacing

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "YAAP"
                color: Theme.accent
                font.pixelSize: 15
                font.bold: true
                font.letterSpacing: 3
            }
            Item { Layout.fillWidth: true }
            Button { text: "Mods"; onClicked: modsDialog.open() }
            Label { text: Player.stateName; color: Theme.secondaryText }
        }

        ExtensionHost {
            slotId: "navigation.primary"
            Layout.fillWidth: true
        }

        Item { Layout.fillHeight: true }

        Label {
            Layout.fillWidth: true
            text: Player.title
            color: Theme.primaryText
            font.pixelSize: 30
            font.weight: Font.DemiBold
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
            Button { text: "Open file"; enabled: !Player.isLoading; onClicked: fileDialog.open() }
            Button { text: "Open stream"; enabled: !Player.isLoading; onClicked: streamDialog.open() }
            Button {
                text: MusicLibrary.scanning ? "Scanning…" : "Add library (" + MusicLibrary.trackCount + ")"
                enabled: !MusicLibrary.scanning
                onClicked: folderDialog.open()
            }
            Button {
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
            text: "Extension API " + ModApi.version + " · FFmpeg → bounded PCM stream → miniaudio"
            color: Theme.secondaryText
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
