import QtQuick
import QtQuick.Controls
import Yaap.ModApi 1.0

Item {
    id: root

    required property var hostWindow
    property bool miniPlayer: false

    signal closeRequested()

    readonly property int buttonWidth: miniPlayer
        ? Theme.miniWindowControlWidth : Theme.closeButtonWidth
    readonly property int buttonHeight: miniPlayer
        ? Theme.miniWindowControlHeight : Theme.closeButtonHeight
    readonly property int rightInset: miniPlayer
        ? Theme.miniWindowControlsRightInset : Theme.closeButtonRightInset
    readonly property int topInset: miniPlayer
        ? Theme.miniWindowControlsTopInset : Theme.closeButtonTopInset
    readonly property int buttonSpacing: miniPlayer
        ? Theme.miniWindowControlSpacing : Math.max(4, Theme.spacing / 2)

    ToolButton {
        id: closeButton

        anchors.right: parent.right
        anchors.rightMargin: root.rightInset
        anchors.top: parent.top
        anchors.topMargin: root.topInset
        width: root.buttonWidth
        height: root.buttonHeight
        text: "×"
        flat: true
        font.pixelSize: 20
        Accessible.name: "Close Yaap"
        ToolTip.visible: hovered
        ToolTip.text: "Close"
        onClicked: root.closeRequested()

        background: Rectangle {
            color: closeButton.down
                ? Qt.darker(Theme.error, 1.15)
                : closeButton.hovered ? Theme.error : "transparent"
        }
    }

    ToolButton {
        id: presentationButton

        anchors.right: closeButton.left
        anchors.rightMargin: root.buttonSpacing
        anchors.top: closeButton.top
        width: root.buttonWidth
        height: root.buttonHeight
        flat: true
        display: AbstractButton.IconOnly
        icon.source: root.miniPlayer
            ? "icons/restore.svg" : "icons/miniplayer.svg"
        icon.color: Theme.primaryText
        Accessible.name: root.miniPlayer
            ? "Restore full player" : "Switch to miniplayer"
        ToolTip.visible: hovered
        ToolTip.text: root.miniPlayer
            ? "Restore full player" : "Switch to miniplayer"
        onClicked: {
            if (root.miniPlayer)
                root.hostWindow.restoreFullPlayer()
            else
                root.hostWindow.enterMiniPlayer()
        }

        background: Rectangle {
            radius: Math.min(Theme.cornerRadius, 8)
            color: presentationButton.down
                ? Qt.darker(Theme.accent, 1.25)
                : presentationButton.hovered
                    ? Qt.rgba(Theme.accent.r, Theme.accent.g,
                        Theme.accent.b, 0.28) : "transparent"
        }
    }
}
