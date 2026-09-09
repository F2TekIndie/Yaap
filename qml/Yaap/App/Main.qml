import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import Yaap.App 1.0
import Yaap.ModApi 1.0
import Yaap.Ui 1.0

ApplicationWindow {
    id: root

    // PROTOTYPE: FramelessDialog uses this counter for QML-side modality.
    // Replace it with the planned shared window manager when dialog stacking
    // and ownership are centralized.
    property int framelessModalDepth: 0
    property bool applicationClosing: false
    property bool presentationTransitioning: false
    property bool presentationInitialized: false

    width: 900
    height: 560
    minimumWidth: 680
    minimumHeight: 420
    visible: false
    flags: Qt.Window | Qt.FramelessWindowHint
    title: "Yaap — Music player prototype"
    color: "transparent"
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

    function currentAvailableGeometry() {
        return Presentation.availableGeometryForWindow(
            Qt.rect(root.x, root.y, root.width, root.height))
    }

    function applyWindowGeometry(geometry) {
        root.x = geometry.x
        root.y = geometry.y
        root.width = geometry.width
        root.height = geometry.height
    }

    function applyPresentationConstraints(miniPlayer) {
        if (miniPlayer) {
            root.minimumWidth = Theme.miniPlayerWidth
            root.minimumHeight = Theme.miniPlayerHeight
            root.maximumWidth = Theme.miniPlayerWidth
            root.maximumHeight = Theme.miniPlayerHeight
        } else {
            root.maximumWidth = 16777215
            root.maximumHeight = 16777215
            root.minimumWidth = 680
            root.minimumHeight = 420
        }
    }

    function enterMiniPlayer() {
        if (presentationTransitioning || Presentation.miniPlayer)
            return

        presentationTransitioning = true
        const available = currentAvailableGeometry()
        const normalGeometry = Qt.rect(root.x, root.y, root.width, root.height)
        Presentation.enterMiniPlayer(normalGeometry, available)
        applyPresentationConstraints(true)
        applyWindowGeometry(Presentation.miniPlayerGeometryFor(available))
        presentationTransitioning = false
        Qt.callLater(function() { miniPlayerView.focusTransport() })
    }

    function restoreFullPlayer() {
        if (presentationTransitioning || !Presentation.miniPlayer)
            return

        presentationTransitioning = true
        const geometry = Presentation.normalGeometryFor(currentAvailableGeometry())
        Presentation.restoreFullPlayer()
        applyPresentationConstraints(false)
        applyWindowGeometry(geometry)
        presentationTransitioning = false
        Qt.callLater(function() { fullPlayerView.focusTransport() })
    }

    function savePresentationGeometry() {
        if (!presentationInitialized || presentationTransitioning)
            return
        const available = currentAvailableGeometry()
        if (Presentation.miniPlayer) {
            Presentation.recordMiniPlayerPosition(Qt.point(root.x, root.y), available)
        } else {
            Presentation.recordNormalGeometry(
                Qt.rect(root.x, root.y, root.width, root.height), available)
        }
    }

    function reclampPresentationGeometry() {
        if (!presentationInitialized || presentationTransitioning)
            return
        savePresentationGeometry()
        const geometry = Presentation.miniPlayer
            ? Presentation.miniPlayerGeometryFor(currentAvailableGeometry())
            : Presentation.normalGeometryFor(currentAvailableGeometry())
        presentationTransitioning = true
        applyWindowGeometry(geometry)
        presentationTransitioning = false
    }

    Component.onCompleted: {
        applyPresentationConstraints(Presentation.miniPlayer)
        applyWindowGeometry(Presentation.initialGeometry)
        presentationInitialized = true
    }

    onXChanged: if (presentationInitialized && !presentationTransitioning)
        geometrySaveTimer.restart()
    onYChanged: if (presentationInitialized && !presentationTransitioning)
        geometrySaveTimer.restart()
    onWidthChanged: if (presentationInitialized && !presentationTransitioning)
        geometrySaveTimer.restart()
    onHeightChanged: if (presentationInitialized && !presentationTransitioning)
        geometrySaveTimer.restart()
    onScreenChanged: Qt.callLater(reclampPresentationGeometry)
    onClosing: savePresentationGeometry()

    Timer {
        id: geometrySaveTimer
        interval: 350
        repeat: false
        onTriggered: root.savePresentationGeometry()
    }

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

    FramelessDialog {
        id: streamDialog
        settingsKey: "stream"
        defaultWidth: 560
        defaultHeight: 320
        minimumWidth: 480
        minimumHeight: 280
        title: "Open internet radio or provider stream"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: playlistMode.checked
            ? Player.openRadioPlaylist(streamUrl.text)
            : Player.openStream(streamUrl.text, streamName.text, liveStream.checked)

        contentItem: ColumnLayout {
            Label { text: "HTTP(S) stream URL" }
            TextField {
                id: streamUrl
                Layout.preferredWidth: 440
                placeholderText: "https://radio.example/live"
            }
            Label { text: "Display name (optional)" }
            TextField { id: streamName; Layout.fillWidth: true }
            CheckBox {
                id: liveStream
                text: "Live radio (reconnect when the stream ends)"
                checked: true
                enabled: !playlistMode.checked
            }
            CheckBox {
                id: playlistMode
                text: "URL is an M3U, PLS, or XSPF station playlist"
            }
        }
    }

    FramelessDialog {
        id: trustDialog
        settingsKey: "trust-mod"
        defaultWidth: 600
        defaultHeight: 390
        minimumWidth: 520
        minimumHeight: 340
        property string pendingModId
        property var requestedPermissions: []
        property bool activateThemeAfterGrant: false
        property string pendingPublisher
        property string pendingDigest

        title: "Trust third-party mod?"
        modal: true
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

        contentItem: ColumnLayout {
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

    FramelessDialog {
        id: modsDialog
        settingsKey: "mods"
        defaultWidth: 760
        defaultHeight: 520
        minimumWidth: 640
        minimumHeight: 420
        title: "Mods and themes — API " + ModApi.version
        modal: true
        standardButtons: Dialog.Close

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
                        RowLayout {
                            Layout.fillWidth: true
                            visible: isTheme
                                && Theme.currentThemeId === modId
                                && Theme.spectrumHueShiftAdjustable

                            Label {
                                text: "Background hue"
                                color: Theme.secondaryText
                            }
                            Slider {
                                id: hueShiftSlider

                                Layout.fillWidth: true
                                from: -180
                                to: 180
                                stepSize: 1
                                Accessible.name: "Synthwave background hue shift"
                                onMoved: Theme.spectrumHueShiftDegrees = value
                                onPressedChanged: if (!pressed) Theme.commitSpectrumHueShift()

                                Binding on value {
                                    when: !hueShiftSlider.pressed
                                    value: Theme.spectrumHueShiftDegrees
                                }
                            }
                            Label {
                                Layout.preferredWidth: 48
                                text: Math.round(hueShiftSlider.value) + "°"
                                color: Theme.primaryText
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }
                }
            }

            Button { text: "Rescan packages"; onClicked: Mods.refresh() }
        }
    }

    FramelessDialog {
        id: providersDialog
        settingsKey: "providers"
        defaultWidth: 780
        defaultHeight: 560
        minimumWidth: 640
        minimumHeight: 420
        title: "Providers"
        modal: true
        standardButtons: Dialog.Close

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

    FramelessDialog {
        id: accountsDialog
        settingsKey: "provider-accounts"
        defaultWidth: 780
        defaultHeight: 600
        minimumWidth: 640
        minimumHeight: 500
        title: "Provider accounts"
        modal: true
        standardButtons: Dialog.Close

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

    FramelessDialog {
        id: libraryDialog
        settingsKey: "library"
        defaultWidth: 780
        defaultHeight: 560
        minimumWidth: 640
        minimumHeight: 420
        title: "Local library"
        modal: true
        standardButtons: Dialog.Close
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

    FramelessDialog {
        id: radioDialog
        settingsKey: "radio"
        defaultWidth: 820
        defaultHeight: 600
        minimumWidth: 680
        minimumHeight: 480
        title: "Internet radio"
        modal: true
        standardButtons: Dialog.Close
        onOpened: {
            radioTabs.currentIndex = Radio.count === 0 ? 1 : 0
            directorySearch.clear()
            RadioDirectory.refreshPopular()
        }
        onRejected: RadioDirectory.cancel()
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
                            required property string stationCountryCode
                            required property string stationLanguage
                            required property string stationTags
                            required property string stationCodec
                            required property int stationBitrate
                            required property bool stationHls
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
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        visible: text.length > 0
                                        text: [stationCountryCode, stationLanguage,
                                            stationCodec + (stationBitrate > 0
                                                ? " " + stationBitrate + " kbps" : ""),
                                            stationHls ? "HLS" : ""]
                                            .filter(value => value.length > 0).join(" · ")
                                        color: Theme.secondaryText
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        visible: stationTags.length > 0
                                        text: stationTags
                                        color: Theme.secondaryText
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
                                        onClicked: RadioDirectory.save(index)
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

    Image {
        id: themeBackgroundImage

        anchors.fill: parent
        enabled: false
        visible: source.toString().length > 0
        source: Presentation.miniPlayer
            ? Theme.miniBackgroundImageSource : Theme.backgroundImageSource
        opacity: Presentation.miniPlayer
            ? Theme.miniBackgroundImageOpacity : Theme.backgroundImageOpacity
        asynchronous: true
        cache: true
        smooth: true
        mipmap: true
        fillMode: (Presentation.miniPlayer
                ? Theme.miniBackgroundImageFit : Theme.backgroundImageFit) === "stretch"
            ? Image.Stretch
            : (Presentation.miniPlayer
                    ? Theme.miniBackgroundImageFit : Theme.backgroundImageFit)
                    === "preserveAspectCrop"
                ? Image.PreserveAspectCrop : Image.PreserveAspectFit
        readonly property string imageAlignment: Presentation.miniPlayer
            ? Theme.miniBackgroundImageAlignment : Theme.backgroundImageAlignment
        horizontalAlignment: imageAlignment === "left"
                || imageAlignment === "top-left"
                || imageAlignment === "bottom-left"
            ? Image.AlignLeft
            : imageAlignment === "right"
                    || imageAlignment === "top-right"
                    || imageAlignment === "bottom-right"
                ? Image.AlignRight : Image.AlignHCenter
        verticalAlignment: imageAlignment === "top"
                || imageAlignment === "top-left"
                || imageAlignment === "top-right"
            ? Image.AlignTop
            : imageAlignment === "bottom"
                    || imageAlignment === "bottom-left"
                    || imageAlignment === "bottom-right"
                ? Image.AlignBottom : Image.AlignVCenter
    }

    Loader {
        anchors.fill: parent
        active: !Presentation.miniPlayer && Theme.backgroundEffect === "waves"
        sourceComponent: Component {
            AnimatedWaveBackground {
                anchors.fill: parent
            }
        }
    }

    Loader {
        anchors.fill: parent
        active: !Presentation.miniPlayer && Theme.backgroundEffect === "spectrum"
        sourceComponent: Component {
            SpectrumBackground {
                anchors.fill: parent
            }
        }
    }

    Loader {
        anchors.fill: parent
        active: !Presentation.miniPlayer && Theme.backgroundEffect === "paperPlanes"
        sourceComponent: Component {
            AnimatedPaperPlaneBackground {
                anchors.fill: parent
            }
        }
    }

    MouseArea {
        // Keep this behind the presentation views. Their buttons, sliders, and
        // extension controls receive input first; any genuinely unused visual
        // area falls through to this native window-drag surface.
        anchors.fill: parent
        enabled: root.framelessModalDepth === 0
        acceptedButtons: Qt.LeftButton
        cursorShape: pressed ? Qt.ClosedHandCursor : Qt.ArrowCursor
        onPressed: root.startSystemMove()
    }

    WindowChrome {
        anchors.fill: parent
        z: 10
        hostWindow: root
        miniPlayer: Presentation.miniPlayer
        onCloseRequested: root.close()
    }

    FullPlayerView {
        id: fullPlayerView
        anchors.fill: parent
        anchors.leftMargin: Theme.controlAreaLeftInset
        anchors.rightMargin: Theme.controlAreaRightInset
        anchors.topMargin: Theme.controlAreaTopInset
        anchors.bottomMargin: Theme.controlAreaBottomInset
        visible: !Presentation.miniPlayer
        enabled: visible
        hostWindow: root
        onProvidersRequested: providersDialog.open()
        onAccountsRequested: accountsDialog.open()
        onLibraryRequested: libraryDialog.open()
        onRadioRequested: radioDialog.open()
        onModsRequested: modsDialog.open()
        onFileRequested: fileDialog.open()
        onStreamRequested: streamDialog.open()
    }

    MiniPlayerView {
        id: miniPlayerView
        anchors.fill: parent
        visible: Presentation.miniPlayer
        enabled: visible
        hostWindow: root
    }
}
