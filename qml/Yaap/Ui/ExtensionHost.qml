import QtQuick
import QtQuick.Layouts
import Yaap.ModApi 1.0

ColumnLayout {
    id: host

    required property string slotId
    spacing: Theme.spacing
    visible: children.length > 0

    Repeater {
        model: Extensions

        Loader {
            required property string slotId
            required property url componentSource
            required property string modId

            Layout.fillWidth: true
            active: slotId === host.slotId
            source: componentSource
            asynchronous: true
            onStatusChanged: {
                if (status === Loader.Error)
                    console.warn("Extension component failed:", modId, componentSource)
            }
        }
    }
}
