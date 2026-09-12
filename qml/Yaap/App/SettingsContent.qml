import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Yaap.App 1.0
import Yaap.ModApi 1.0

ColumnLayout {
    id: settings
    spacing: 12
    property var draft: ({})
    property string errorMessage: ""
    property string imageKey: ""
    property string colorKey: ""
    // Skin geometry stays with the source theme; expose appearance choices only.
    readonly property var visibleCustomFields: Theme.customFields.filter(function(field) {
        const common = ["windowTop", "windowBottom", "surface", "primaryText",
            "secondaryText", "accent", "error", "cornerRadius", "spacing",
            "backgroundEffect", "miniBackgroundEffect"]
        const spectrum = ["spectrumGradientStart", "spectrumGradientMiddle",
            "spectrumGradientEnd", "spectrumOpacity"]
        return common.indexOf(field.key) !== -1
            || ((draft.backgroundEffect === "spectrum" || draft.miniBackgroundEffect === "spectrum")
                && spectrum.indexOf(field.key) !== -1)
    })

    function loadDraft() { draft = Object.assign({}, Theme.customValues) }
    function optionLabel(value) {
        const labels = { followTheme: "Follow theme", none: "None", waves: "Waves", paperPlanes: "Paper planes",
            spectrum: "Spectrum", preserveAspectFit: "Fit inside", preserveAspectCrop: "Fill and crop",
            stretch: "Stretch", center: "Center", top: "Top", "top-left": "Top left",
            "top-right": "Top right", left: "Left", right: "Right", bottom: "Bottom",
            "bottom-left": "Bottom left", "bottom-right": "Bottom right" }
        return labels[value] || value
    }
    function change(key, value) {
        const updated = Object.assign({}, draft)
        updated[key] = value
        draft = updated
    }
    function syncSelection() {
        for (let i = 0; i < themeChoice.count; ++i) {
            if (themeChoice.valueAt(i) === Theme.currentThemeId) {
                themeChoice.currentIndex = i
                return
            }
        }
    }
    Component.onCompleted: { loadDraft(); syncSelection() }
    Connections {
        target: Theme
        function onThemeChanged() { settings.syncSelection(); settings.loadDraft() }
        function onAvailableThemesChanged() { settings.syncSelection() }
    }

    TabBar {
        id: sections
        objectName: "settingsSections"
        Layout.fillWidth: true
        TabButton { text: "General" }
        TabButton { text: "Themes" }
    }
    StackLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        currentIndex: sections.currentIndex
        ColumnLayout {
            spacing: 12
            Label { color: Theme.primaryText; text: "Runtime behavior"; font.bold: true }
            CheckBox {
                objectName: "backgroundBehavior"
                palette.windowText: Theme.primaryText
                text: "Keep playing when the window closes"
                visible: Session.backgroundAvailable
                checked: Session.keepPlayingInBackground
                onToggled: Session.keepPlayingInBackground = checked
            }
            Label { color: Theme.primaryText;
                Layout.fillWidth: true
                text: Session.backgroundAvailable
                    ? "With background playback enabled, closing the window keeps Yaap running. Open Yaap again to restore the window."
                    : "Background playback is unavailable in this session. Closing the window quits Yaap."
                wrapMode: Text.Wrap
            }
            Button { objectName: "quitNow"; text: "Quit Now"; onClicked: Session.quit() }
            Item { Layout.fillHeight: true }
        }
        ColumnLayout {
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                Label { color: Theme.primaryText; text: "Theme" }
                ComboBox {
                    id: themeChoice
                    objectName: "themeChoice"
                    Layout.fillWidth: true
                    model: Theme.availableThemes
                    textRole: "name"
                    valueRole: "id"
                    Accessible.name: "Theme"
                    onActivated: {
                        settings.errorMessage = Theme.useTheme(currentValue)
                        settings.syncSelection()
                    }
                }
                Button { text: "Reload themes"; onClicked: Mods.refresh() }
            }
            RowLayout {
                Layout.fillWidth: true
                visible: Theme.currentThemeId !== "builtin.custom"
                Label { text: "Miniplayer background"; color: Theme.primaryText }
                ComboBox {
                    objectName: "miniBackgroundChoice"
                    Layout.fillWidth: true
                    model: ["followTheme", "none", "waves", "paperPlanes", "spectrum"]
                    currentIndex: model.indexOf(Theme.miniBackgroundEffect)
                    displayText: settings.optionLabel(currentText)
                    delegate: ItemDelegate {
                        required property string modelData
                        width: parent ? parent.width : implicitWidth
                        text: settings.optionLabel(modelData)
                    }
                    onActivated: Theme.miniBackgroundEffect = currentText
                    Accessible.name: "Miniplayer background effect"
                }
            }
            Label {
                Layout.fillWidth: true
                visible: settings.errorMessage.length > 0
                text: settings.errorMessage
                color: Theme.error
                wrapMode: Text.Wrap
            }
            Label { color: Theme.primaryText;
                Layout.fillWidth: true
                visible: Theme.currentThemeId !== "builtin.custom"
                text: Theme.currentThemeId === "builtin.dms" ? Theme.dmsStatus
                    : "Choose Custom to adjust colors, corner radius, spacing, and animation."
                wrapMode: Text.Wrap
            }
            ScrollView {
                id: customScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: Theme.currentThemeId === "builtin.custom"
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ColumnLayout {
                    width: customScroll.availableWidth
                    spacing: 10
                    Repeater {
                        model: settings.visibleCustomFields
                        delegate: ColumnLayout {
                            id: field
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            spacing: 4
                            Label { color: Theme.primaryText;
                                visible: field.index === 0
                                    || settings.visibleCustomFields[field.index - 1].group !== field.modelData.group
                                text: field.modelData.group
                                font.bold: true
                                font.pixelSize: 18
                                topPadding: 12
                            }
                            Label { color: Theme.primaryText; text: field.modelData.label; Layout.fillWidth: true }
                            Loader {
                                objectName: "field-" + field.modelData.key
                                Layout.fillWidth: true
                                sourceComponent: field.modelData.type === "boolean" ? booleanEditor
                                    : field.modelData.type === "choice" ? choiceEditor
                                    : field.modelData.type === "image" ? imageEditor
                                    : field.modelData.type === "integer" ? integerEditor
                                    : field.modelData.type === "color" ? colorEditor : numberEditor
                            }
                            Component {
                                id: booleanEditor
                                CheckBox {
                                    text: "On"
                                    palette.windowText: Theme.primaryText
                                    checked: settings.draft[field.modelData.key] === true
                                    onToggled: settings.change(field.modelData.key, checked)
                                    Accessible.name: field.modelData.label
                                }
                            }
                            Component {
                                id: choiceEditor
                                ComboBox {
                                    model: field.modelData.choices
                                    currentIndex: model.indexOf(settings.draft[field.modelData.key])
                                    displayText: settings.optionLabel(currentText)
                                    delegate: ItemDelegate {
                                        required property string modelData
                                        width: parent ? parent.width : implicitWidth
                                        text: settings.optionLabel(modelData)
                                    }
                                    onActivated: settings.change(field.modelData.key, currentText)
                                    Accessible.name: field.modelData.label
                                }
                            }
                            Component {
                                id: integerEditor
                                SpinBox {
                                    objectName: "editor-" + field.modelData.key
                                    from: field.modelData.minimum
                                    to: field.modelData.maximum
                                    value: Number(settings.draft[field.modelData.key] || 0)
                                    editable: from !== to
                                    enabled: from !== to
                                    ToolTip.visible: hovered && from === to
                                    ToolTip.text: "The miniplayer currently uses a fixed size."
                                    onValueModified: settings.change(field.modelData.key, value)
                                    Accessible.name: field.modelData.label
                                }
                            }
                            Component {
                                id: numberEditor
                                TextField {
                                    text: String(settings.draft[field.modelData.key] ?? "")
                                    placeholderText: field.modelData.minimum + " to " + field.modelData.maximum
                                    onTextEdited: settings.change(field.modelData.key, text)
                                    Accessible.name: field.modelData.label
                                }
                            }
                            Component {
                                id: colorEditor
                                RowLayout {
                                    TextField {
                                        Layout.fillWidth: true
                                        text: String(settings.draft[field.modelData.key] ?? "")
                                        placeholderText: "#AARRGGBB"
                                        onTextEdited: settings.change(field.modelData.key, text)
                                        Accessible.name: field.modelData.label
                                    }
                                    Button {
                                        text: "Pick color"
                                        onClicked: {
                                            settings.colorKey = field.modelData.key
                                            colorPicker.selectedColor = settings.draft[field.modelData.key]
                                            colorPicker.open()
                                        }
                                    }
                                }
                            }
                            Component {
                                id: imageEditor
                                RowLayout {
                                    TextField {
                                        Layout.fillWidth: true
                                        readOnly: true
                                        text: String(settings.draft[field.modelData.key] ?? "")
                                        placeholderText: "No image"
                                        Accessible.name: field.modelData.label
                                    }
                                    Button {
                                        text: "Browse"
                                        onClicked: { settings.imageKey = field.modelData.key; imagePicker.open() }
                                    }
                                    Button { text: "Clear"; onClicked: settings.change(field.modelData.key, "") }
                                }
                            }
                        }
                    }
                }
            }
            RowLayout {
                visible: Theme.currentThemeId === "builtin.custom"
                Button {
                    text: "Apply custom theme"
                    objectName: "applyCustom"
                    onClicked: settings.errorMessage = Theme.applyCustom(settings.draft)
                }
                Button { text: "Revert edits"; onClicked: { settings.loadDraft(); settings.errorMessage = "" } }
                Label { color: Theme.primaryText; text: "Changes are saved when applied." }
            }
            Item { Layout.fillHeight: true; visible: Theme.currentThemeId !== "builtin.custom" }
        }
    }
    FileDialog {
        id: imagePicker
        title: "Choose a background image"
        nameFilters: ["Images (*.png *.svg)"]
        onAccepted: settings.change(settings.imageKey, selectedFile.toString())
    }
    ColorDialog {
        id: colorPicker
        title: "Choose a theme color"
        options: ColorDialog.ShowAlphaChannel
        onAccepted: settings.change(settings.colorKey, selectedColor.toString())
    }
}
