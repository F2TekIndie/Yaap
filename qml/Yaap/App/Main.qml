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
    flags: Qt.Window | Qt.FramelessWindowHint
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
        property string pendingPublisher
        property string pendingDigest

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
            Label {
                Layout.fillWidth: true
                text: "Declared publisher: " + trustDialog.pendingPublisher
                    + "\nSHA-256: " + trustDialog.pendingDigest
                color: Theme.secondaryText
                wrapMode: Text.WrapAnywhere
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
                    required property string contentDigest
                    required property string publisher

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
                                        trustDialog.pendingPublisher = publisher
                                        trustDialog.pendingDigest = contentDigest
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
                                        trustDialog.pendingPublisher = publisher
                                        trustDialog.pendingDigest = contentDigest
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

    Dialog {
        id: providersDialog
        title: "Providers"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Close
        width: 720
        height: 500

        contentItem: ColumnLayout {
            spacing: Theme.spacing

            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: providerSearch
                    Layout.fillWidth: true
                    placeholderText: "Search enabled providers"
                    Accessible.name: placeholderText
                    onAccepted: Providers.search(text)
                }
                Button {
                    text: "Search"
                    enabled: Providers.availableProviderCount > 0 && !Providers.loading
                    onClicked: Providers.search(providerSearch.text)
                }
                CheckBox {
                    text: "Offline"
                    checked: Providers.offlineMode
                    onToggled: Providers.offlineMode = checked
                }
            }

            Label {
                Layout.fillWidth: true
                visible: Providers.availableProviderCount === 0
                text: "Enable and trust a provider in Mods, then return here."
                color: Theme.secondaryText
                wrapMode: Text.Wrap
            }
            Label {
                Layout.fillWidth: true
                visible: Providers.errorMessage.length > 0
                text: Providers.errorMessage
                color: Theme.error
                wrapMode: Text.Wrap
            }
            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: Providers.loading
                visible: running
            }

            ListView {
                id: providerResults
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: Theme.spacing
                model: Providers

                delegate: ThemedPanel {
                    required property string trackId
                    required property string title
                    required property string artist
                    required property string album
                    required property string providerId
                    required property int durationMilliseconds

                    width: ListView.view.width
                    height: providerDetails.implicitHeight + 24

                    RowLayout {
                        id: providerDetails
                        anchors.fill: parent
                        anchors.margins: 12
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label {
                                Layout.fillWidth: true
                                text: title
                                color: Theme.primaryText
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: (artist.length > 0 ? artist : "Unknown artist")
                                    + " · " + providerId
                                color: Theme.secondaryText
                                elide: Text.ElideRight
                            }
                        }
                        Button {
                            text: "Play"
                            Accessible.name: "Play " + title
                            onClicked: Providers.play(index)
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: accountsDialog
        title: "Provider accounts"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Close
        width: 720
        height: 560

        contentItem: ColumnLayout {
            spacing: Theme.spacing

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                Label { text: "Provider"; color: Theme.secondaryText }
                ComboBox {
                    id: accountProvider
                    Layout.fillWidth: true
                    model: ["OpenSubsonic", "Jellyfin"]
                }
                Label { text: "Name"; color: Theme.secondaryText }
                TextField { id: accountName; Layout.fillWidth: true; placeholderText: "Home server" }
                Label { text: "Server"; color: Theme.secondaryText }
                TextField { id: accountServer; Layout.fillWidth: true; placeholderText: "https://music.example" }
                Label { text: "Username"; color: Theme.secondaryText }
                TextField { id: accountUsername; Layout.fillWidth: true }
                Label { text: "Password"; color: Theme.secondaryText }
                TextField {
                    id: accountPassword
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                }
            }
            Button {
                text: "Add account securely"
                onClicked: {
                    const providerId = accountProvider.currentIndex === 0
                        ? "opensubsonic" : "jellyfin"
                    if (ProviderAccounts.addAccount(providerId, accountName.text,
                            accountServer.text, accountUsername.text, accountPassword.text)) {
                        accountPassword.clear()
                        accountName.clear()
                        accountServer.clear()
                        accountUsername.clear()
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                visible: ProviderAccounts.errorMessage.length > 0
                text: ProviderAccounts.errorMessage
                color: Theme.error
                wrapMode: Text.Wrap
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: Theme.spacing
                model: ProviderAccounts
                delegate: ThemedPanel {
                    required property string accountId
                    required property string providerId
                    required property string displayName
                    required property url serverUrl
                    required property string username
                    required property bool accountEnabled
                    required property string status
                    width: ListView.view.width
                    height: accountDetails.implicitHeight + 24
                    ColumnLayout {
                        id: accountDetails
                        anchors.fill: parent
                        anchors.margins: 12
                        Label {
                            text: displayName + " · " + providerId
                            color: Theme.primaryText
                            font.bold: true
                        }
                        Label {
                            Layout.fillWidth: true
                            text: username + " @ " + serverUrl + (status.length > 0 ? " — " + status : "")
                            color: Theme.secondaryText
                            elide: Text.ElideRight
                        }
                        RowLayout {
                            Button {
                                text: accountEnabled ? "Disable" : "Enable"
                                onClicked: ProviderAccounts.setEnabled(accountId, !accountEnabled)
                            }
                            Button {
                                text: "Test connection"
                                enabled: accountEnabled
                                onClicked: ProviderAccounts.testConnection(accountId)
                            }
                            Button {
                                text: "Remove"
                                onClicked: ProviderAccounts.removeAccount(accountId)
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: libraryDialog
        title: "Local library"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Close
        width: 720
        height: 500
        contentItem: ColumnLayout {
            RowLayout {
                Layout.fillWidth: true
                Button { text: "Add folder"; onClicked: folderDialog.open() }
                Button { text: "Rescan"; enabled: !MusicLibrary.scanning; onClicked: MusicLibrary.rescan() }
                Label {
                    text: MusicLibrary.scanning ? "Scanning…" : MusicLibrary.trackCount + " tracks"
                    color: Theme.secondaryText
                }
            }
            Label {
                Layout.fillWidth: true
                visible: MusicLibrary.errorMessage.length > 0
                text: MusicLibrary.errorMessage
                color: Theme.error
                wrapMode: Text.Wrap
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: Theme.spacing
                model: MusicLibrary
                delegate: ThemedPanel {
                    required property string title
                    required property string artist
                    required property string album
                    width: ListView.view.width
                    height: libraryDetails.implicitHeight + 24
                    RowLayout {
                        id: libraryDetails
                        anchors.fill: parent
                        anchors.margins: 12
                        ColumnLayout {
                            Layout.fillWidth: true
                            Label { text: title; color: Theme.primaryText; font.bold: true }
                            Label {
                                text: (artist.length > 0 ? artist : "Unknown artist")
                                    + (album.length > 0 ? " · " + album : "")
                                color: Theme.secondaryText
                            }
                        }
                        Button { text: "Play"; onClicked: MusicLibrary.play(index) }
                    }
                }
            }
        }
    }

    Dialog {
        id: radioDialog
        title: "Internet radio"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Close
        width: 780
        height: 560
        onOpened: {
            radioTabs.currentIndex = Radio.count === 0 ? 1 : 0
            directorySearch.clear()
            RadioDirectory.refreshPopular()
        }
        onClosed: RadioDirectory.cancel()
        contentItem: ColumnLayout {
            TabBar {
                id: radioTabs
                Layout.fillWidth: true
                TabButton { text: "My stations (" + Radio.count + ")" }
                TabButton { text: "Browse Radio Browser" }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: radioTabs.currentIndex

                ColumnLayout {
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        TextField { id: stationName; Layout.fillWidth: true; placeholderText: "Station name" }
                        TextField { id: stationUrl; Layout.fillWidth: true; placeholderText: "https://…/stream" }
                        Button {
                            text: "Add station"
                            onClicked: if (Radio.addStation(stationName.text, stationUrl.text)) {
                                stationName.clear(); stationUrl.clear()
                            }
                        }
                        Item { Layout.fillWidth: true }
                        TextField {
                            id: playlistUrl
                            Layout.fillWidth: true
                            placeholderText: "M3U, PLS, or XSPF playlist URL"
                        }
                        Button { text: "Import playlist"; onClicked: Radio.importPlaylist(playlistUrl.text) }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: Radio.errorMessage.length > 0
                        text: Radio.errorMessage
                        color: Theme.error
                        wrapMode: Text.Wrap
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: Radio.count === 0
                        text: "No saved stations yet. Browse Radio Browser or add a stream URL."
                        color: Theme.secondaryText
                        horizontalAlignment: Text.AlignHCenter
                    }
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: Theme.spacing
                        model: Radio
                        delegate: ThemedPanel {
                            required property int index
                            required property string stationName
                            required property url streamUrl
                            width: ListView.view.width
                            height: savedRadioDetails.implicitHeight + 24
                            RowLayout {
                                id: savedRadioDetails
                                anchors.fill: parent
                                anchors.margins: 12
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    Label {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        text: stationName
                                        color: Theme.primaryText
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        id: savedStationUrl
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        text: streamUrl.toString()
                                        color: Theme.secondaryText
                                        elide: Text.ElideRight

                                        HoverHandler { id: savedStationUrlHover }
                                        ToolTip {
                                            visible: savedStationUrlHover.hovered
                                            delay: 400
                                            timeout: 15000
                                            width: Math.min(implicitWidth, radioDialog.width - 48)
                                            contentItem: Label {
                                                text: streamUrl.toString()
                                                color: Theme.primaryText
                                                wrapMode: Text.WrapAnywhere
                                            }
                                        }
                                    }
                                }
                                RowLayout {
                                    Layout.minimumWidth: implicitWidth
                                    Layout.maximumWidth: implicitWidth
                                    Button { text: "Play"; onClicked: Radio.play(index) }
                                    Button { text: "Remove"; onClicked: Radio.removeStation(index) }
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    RowLayout {
                        Layout.fillWidth: true
                        TextField {
                            id: directorySearch
                            Layout.fillWidth: true
                            placeholderText: "Search station names"
                            onAccepted: RadioDirectory.search(text)
                        }
                        Button {
                            text: "Search"
                            enabled: !RadioDirectory.loading
                            onClicked: RadioDirectory.search(directorySearch.text)
                        }
                        Button {
                            text: RadioDirectory.countryCode.length > 0
                                ? "Popular in " + RadioDirectory.countryCode : "Popular"
                            enabled: !RadioDirectory.loading
                            onClicked: {
                                directorySearch.clear()
                                RadioDirectory.refreshPopular()
                            }
                        }
                        BusyIndicator {
                            running: RadioDirectory.loading
                            visible: running
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: RadioDirectory.errorMessage
                        visible: text.length > 0
                        color: Theme.error
                        wrapMode: Text.Wrap
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: RadioDirectory.stale && RadioDirectory.errorMessage.length === 0
                        text: "Showing cached Radio Browser results while refreshing."
                        color: Theme.secondaryText
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: !RadioDirectory.loading && RadioDirectory.count === 0
                            && RadioDirectory.errorMessage.length === 0
                        text: "No matching working stations were found."
                        color: Theme.secondaryText
                        horizontalAlignment: Text.AlignHCenter
                    }
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: Theme.spacing
                        model: RadioDirectory
                        delegate: ThemedPanel {
                            required property int index
                            required property string stationUuid
                            required property string stationName
                            required property url streamUrl
                            required property string stationCountryCode
                            required property string stationLanguage
                            required property string stationTags
                            required property string stationCodec
                            required property int stationBitrate
                            width: ListView.view.width
                            height: directoryRadioDetails.implicitHeight + 24
                            RowLayout {
                                id: directoryRadioDetails
                                anchors.fill: parent
                                anchors.margins: 12
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    Label {
                                        Layout.fillWidth: true
                                        text: stationName
                                        color: Theme.primaryText
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: [stationCountryCode, stationLanguage,
                                            stationCodec + (stationBitrate > 0
                                                ? " " + stationBitrate + " kbps" : "")]
                                            .filter(value => value.length > 0).join(" · ")
                                        color: Theme.secondaryText
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: stationTags.length > 0
                                        text: stationTags
                                        color: Theme.secondaryText
                                        elide: Text.ElideRight
                                    }
                                }
                                RowLayout {
                                    Layout.minimumWidth: implicitWidth
                                    Layout.maximumWidth: implicitWidth
                                    Button { text: "Play"; onClicked: RadioDirectory.play(index) }
                                    Button {
                                        text: "Save"
                                        onClicked: Radio.addDirectoryStation(
                                            stationName, streamUrl, stationUuid)
                                    }
                                }
                            }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "Station directory provided by Radio Browser"
                        color: Theme.secondaryText
                        horizontalAlignment: Text.AlignRight
                        font.pixelSize: 11
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.windowTop }
            GradientStop { position: 1.0; color: Theme.windowBottom }
        }
    }

    Loader {
        anchors.fill: parent
        active: Theme.backgroundEffect === "waves"
        sourceComponent: Component {
            AnimatedWaveBackground {
                anchors.fill: parent
            }
        }
    }

    Item {
        id: windowChrome

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 36
        z: 10

        MouseArea {
            anchors.left: parent.left
            anchors.right: closeButton.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            acceptedButtons: Qt.LeftButton
            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.ArrowCursor
            onPressed: root.startSystemMove()
        }

        ToolButton {
            id: closeButton

            anchors.right: parent.right
            anchors.top: parent.top
            width: 44
            height: parent.height
            text: "×"
            flat: true
            font.pixelSize: 20
            Accessible.name: "Close Yaap"
            ToolTip.visible: hovered
            ToolTip.text: "Close"
            onClicked: root.close()

            background: Rectangle {
                color: closeButton.down
                    ? Qt.darker(Theme.error, 1.15)
                    : closeButton.hovered ? Theme.error : "transparent"
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 36
        spacing: Theme.spacing

        RowLayout {
            Layout.fillWidth: true
            Item {
                id: windowDragArea

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
                    onPressed: root.startSystemMove()
                }
            }
            Button {
                text: "Providers (" + Providers.availableProviderCount + ")"
                onClicked: providersDialog.open()
            }
            Button {
                text: "Accounts (" + ProviderAccounts.count + ")"
                onClicked: accountsDialog.open()
            }
            Button { text: "Library"; onClicked: libraryDialog.open() }
            Button { text: "Radio (" + Radio.count + ")"; onClicked: radioDialog.open() }
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
