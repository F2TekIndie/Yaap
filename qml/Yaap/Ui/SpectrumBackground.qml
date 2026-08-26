import QtQuick
import Yaap.ModApi 1.1

Item {
    id: root

    clip: true
    enabled: false

    readonly property int columnCount: Math.max(8, Theme.spectrumColumns)

    function sourceBandFor(column) {
        if (AudioVisualization.count <= 1 || columnCount <= 1)
            return 0
        const position = column / (columnCount - 1)
        const spectrumPosition = Theme.spectrumMirror
            ? Math.abs(position * 2 - 1)
            : position
        return Math.round(spectrumPosition * (AudioVisualization.count - 1))
    }

    function mixColor(first, second, amount) {
        const ratio = Math.max(0, Math.min(1, amount))
        return Qt.rgba(
            first.r + (second.r - first.r) * ratio,
            first.g + (second.g - first.g) * ratio,
            first.b + (second.b - first.b) * ratio,
            first.a + (second.a - first.a) * ratio)
    }

    function gradientColorFor(column) {
        if (columnCount <= 1)
            return Theme.spectrumGradientMiddle
        const position = column / (columnCount - 1)
        return position < 0.5
            ? mixColor(Theme.spectrumGradientStart,
                Theme.spectrumGradientMiddle, position * 2)
            : mixColor(Theme.spectrumGradientMiddle,
                Theme.spectrumGradientEnd, (position - 0.5) * 2)
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: parent.height * 0.44
        opacity: Theme.spectrumOpacity * 0.24
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: Qt.rgba(Theme.spectrumGradientStart.r,
                    Theme.spectrumGradientStart.g,
                    Theme.spectrumGradientStart.b, 0)
            }
            GradientStop {
                position: 0.5
                color: Theme.spectrumGradientMiddle
            }
            GradientStop {
                position: 1.0
                color: Qt.rgba(Theme.spectrumGradientEnd.r,
                    Theme.spectrumGradientEnd.g,
                    Theme.spectrumGradientEnd.b, 0)
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: 1
        color: Theme.spectrumGradientMiddle
        opacity: Theme.spectrumOpacity * 0.55
    }

    Row {
        id: spectrumColumns

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        height: parent.height * 0.82
        spacing: Math.max(2, width / root.columnCount * 0.18)

        Repeater {
            model: root.columnCount

            Item {
                id: spectrumColumn

                required property int index
                readonly property int sourceBand: root.sourceBandFor(index)
                readonly property real currentLevel: {
                    const revision = AudioVisualization.revision
                    return revision >= 0
                        ? AudioVisualization.levelAt(sourceBand) : 0
                }
                readonly property real currentPeak: {
                    const revision = AudioVisualization.revision
                    return revision >= 0
                        ? AudioVisualization.peakAt(sourceBand) : 0
                }
                readonly property real halfHeight: (height - 4) / 2
                readonly property real targetHalfHeight: Math.max(2,
                    Math.pow(Math.max(0, currentLevel), 1.25) * halfHeight)
                readonly property color columnColor: root.gradientColorFor(index)

                width: Math.max(2,
                    (spectrumColumns.width
                        - spectrumColumns.spacing * (root.columnCount - 1))
                    / root.columnCount)
                height: spectrumColumns.height

                Item {
                    id: centerLine
                    anchors.centerIn: parent
                    width: parent.width
                    height: 1
                }

                Rectangle {
                    id: upperSpectrumBar

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: centerLine.top
                    height: spectrumColumn.targetHalfHeight
                    radius: Math.min(width / 2, Theme.cornerRadius)
                    opacity: Theme.spectrumOpacity
                    gradient: Gradient {
                        GradientStop {
                            position: 0.0
                            color: Qt.lighter(spectrumColumn.columnColor, 1.35)
                        }
                        GradientStop {
                            position: 1.0
                            color: spectrumColumn.columnColor
                        }
                    }

                    Behavior on height {
                        NumberAnimation {
                            duration: spectrumColumn.targetHalfHeight >= upperSpectrumBar.height
                                ? Theme.spectrumAttackMilliseconds
                                : Theme.spectrumReleaseMilliseconds
                            easing.type: Easing.OutCubic
                        }
                    }
                }

                Rectangle {
                    id: lowerSpectrumBar

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: centerLine.bottom
                    height: spectrumColumn.targetHalfHeight
                    radius: Math.min(width / 2, Theme.cornerRadius)
                    opacity: Theme.spectrumOpacity * 0.86
                    gradient: Gradient {
                        GradientStop {
                            position: 0.0
                            color: spectrumColumn.columnColor
                        }
                        GradientStop {
                            position: 1.0
                            color: Qt.lighter(spectrumColumn.columnColor, 1.22)
                        }
                    }

                    Behavior on height {
                        NumberAnimation {
                            duration: spectrumColumn.targetHalfHeight >= lowerSpectrumBar.height
                                ? Theme.spectrumAttackMilliseconds
                                : Theme.spectrumReleaseMilliseconds
                            easing.type: Easing.OutCubic
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: centerLine.top
                    anchors.bottomMargin: Math.min(spectrumColumn.halfHeight - height,
                        Math.max(0, spectrumColumn.currentPeak) * spectrumColumn.halfHeight)
                    height: 2
                    radius: 1
                    color: Qt.lighter(spectrumColumn.columnColor, 1.45)
                    opacity: Theme.spectrumOpacity * 0.72
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: centerLine.bottom
                    anchors.topMargin: Math.min(spectrumColumn.halfHeight - height,
                        Math.max(0, spectrumColumn.currentPeak) * spectrumColumn.halfHeight)
                    height: 2
                    radius: 1
                    color: Qt.lighter(spectrumColumn.columnColor, 1.35)
                    opacity: Theme.spectrumOpacity * 0.58
                }
            }
        }
    }
}
