import QtQuick
import QtQuick.Controls
import Yaap.ModApi 1.0

ToolButton {
    id: root

    required property url iconSource
    required property string accessibleLabel

    LayoutMirroring.enabled: false
    implicitWidth: 42
    implicitHeight: 38
    display: AbstractButton.IconOnly
    icon.source: root.iconSource
    icon.width: 20
    icon.height: 20
    icon.color: Theme.primaryText
    Accessible.name: root.accessibleLabel
    ToolTip.visible: hovered
    ToolTip.text: root.accessibleLabel

    background: Rectangle {
        radius: Math.min(Theme.cornerRadius, 8)
        color: root.down
            ? Qt.darker(Theme.accent, 1.25)
            : root.hovered
                ? Qt.rgba(Theme.accent.r, Theme.accent.g,
                    Theme.accent.b, 0.28) : "transparent"
    }
}
