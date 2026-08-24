import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: root

    width: 760
    height: 460
    minimumWidth: 560
    minimumHeight: 360
    visible: true
    title: "Yaap — Local playback prototype"
    color: "#111318"

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
        onAccepted: player.openFile(selectedFile)
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#202838" }
            GradientStop { position: 1.0; color: "#101216" }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 36
        spacing: 22

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: "YAAP"
                color: "#80cbc4"
                font.pixelSize: 15
                font.bold: true
                font.letterSpacing: 3
            }

            Item { Layout.fillWidth: true }

            Label {
                text: player.stateName
                color: "#aeb8ca"
                font.pixelSize: 13
            }
        }

        Item { Layout.fillHeight: true }

        Label {
            Layout.fillWidth: true
            text: player.title
            color: "#f4f6fa"
            font.pixelSize: 30
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideMiddle
        }

        Label {
            Layout.fillWidth: true
            visible: player.errorMessage.length > 0
            text: player.errorMessage
            color: "#ff8a80"
            font.pixelSize: 14
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: player.isLoading
            visible: running
        }

        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: 1
            value: Math.max(0, Math.min(1, player.progress))
        }

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: root.formatTime(player.positionMilliseconds)
                color: "#aeb8ca"
            }

            Item { Layout.fillWidth: true }

            Label {
                text: root.formatTime(player.durationMilliseconds)
                color: "#aeb8ca"
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 12

            Button {
                text: "Open file"
                enabled: !player.isLoading
                onClicked: fileDialog.open()
            }

            Button {
                text: player.isPlaying ? "Pause" : "Play"
                enabled: player.hasAudio
                onClicked: player.isPlaying ? player.pause() : player.play()
            }

            Button {
                text: "Stop"
                enabled: player.hasAudio
                onClicked: player.stop()
            }
        }

        Item { Layout.fillHeight: true }

        Label {
            Layout.fillWidth: true
            text: "Prototype: whole-file decoding • FFmpeg → float PCM → miniaudio"
            color: "#718096"
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
        }
    }
}

