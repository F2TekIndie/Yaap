pragma ComponentBehavior: Bound

import QtQuick
import Yaap.ModApi 1.0

Item {
    id: root
    property bool compact: false

    clip: true
    enabled: false

    component FlyingPlane: Item {
        id: plane

        required property int planeIndex
        readonly property real startOffset: Math.random()
        readonly property real speedPixelsPerSecond: 42 + Math.random() * 76
        readonly property real waveAmplitude: (10 + Math.random() * 24) * (root.compact ? 0.4 : 1)
        readonly property real waveCycles: 0.8 + Math.random() * 1.65
        readonly property real wavePhase: Math.random() * Math.PI * 2
        readonly property real verticalCenterRatio: 0.12 + Math.random() * 0.76
        readonly property real travelDistance: root.width + width * 2
        readonly property real normalizedPosition: (travelPhase + startOffset) % 1.0
        readonly property real waveAngle: normalizedPosition * waveCycles
            * Math.PI * 2 + wavePhase
        readonly property real unclampedY: verticalCenterRatio * root.height
            + Math.sin(waveAngle) * waveAmplitude - height / 2
        property real travelPhase: 0
        property color planeFill: planeIndex % 3 === 0
            ? Qt.rgba(Theme.surface.r, Theme.surface.g, Theme.surface.b, 0.72)
            : planeIndex % 3 === 1
                ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.42)
                : Qt.rgba(Theme.windowTop.r, Theme.windowTop.g,
                    Theme.windowTop.b, 0.62)
        property color foldColor: Qt.rgba(Theme.primaryText.r,
            Theme.primaryText.g, Theme.primaryText.b, 0.34)

        width: (28 + Math.random() * 24) * (root.compact ? 0.65 : 1)
        height: width * 0.58
        x: -width + normalizedPosition * travelDistance
        y: Math.max(6, Math.min(root.height - height - 6, unclampedY))
        rotation: Math.max(-24, Math.min(24,
            Math.atan(waveAmplitude * waveCycles * Math.PI * 2
                * Math.cos(waveAngle) / Math.max(1, travelDistance))
                * 180 / Math.PI))
        opacity: 0.44 + Math.random() * 0.32

        Canvas {
            id: planeCanvas

            anchors.fill: parent

            onPaint: {
                const context = getContext("2d")
                const w = width
                const h = height
                context.clearRect(0, 0, w, h)

                // Host-rendered vector plane: theme packages select the effect,
                // but do not execute their own QML or JavaScript.
                context.fillStyle = plane.planeFill
                context.strokeStyle = plane.foldColor
                context.lineWidth = Math.max(1, w / 34)
                context.beginPath()
                context.moveTo(0, 0)
                context.lineTo(w, h * 0.5)
                context.lineTo(0, h)
                context.lineTo(w * 0.27, h * 0.57)
                context.lineTo(w * 0.1, h * 0.5)
                context.lineTo(w * 0.27, h * 0.43)
                context.closePath()
                context.fill()
                context.stroke()

                context.beginPath()
                context.moveTo(w * 0.1, h * 0.5)
                context.lineTo(w, h * 0.5)
                context.moveTo(w * 0.27, h * 0.43)
                context.lineTo(w * 0.64, h * 0.5)
                context.lineTo(w * 0.27, h * 0.57)
                context.stroke()
            }
        }

        onWidthChanged: planeCanvas.requestPaint()
        onHeightChanged: planeCanvas.requestPaint()
        onPlaneFillChanged: planeCanvas.requestPaint()
        onFoldColorChanged: planeCanvas.requestPaint()

        NumberAnimation on travelPhase {
            from: 0
            to: 1
            running: root.visible && root.width > 0 && root.height > 0
            loops: Animation.Infinite
            duration: Math.round(plane.travelDistance
                / plane.speedPixelsPerSecond * 1000)
            easing.type: Easing.Linear
        }
    }

    Repeater {
        model: root.compact ? 5 : 11

        FlyingPlane {
            required property int index
            planeIndex: index
        }
    }
}
