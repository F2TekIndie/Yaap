import QtQuick
import Yaap.ModApi 1.0

Item {
    id: root

    clip: true
    enabled: false

    component WaveBand: Item {
        id: band

        required property color waveColor
        required property real baselineRatio
        required property real amplitude
        required property int travelDuration
        required property bool animationRunning
        property bool reverse: false
        property real phase: 0

        Canvas {
            id: waveCanvas

            x: -band.width * band.phase
            width: band.width * 2
            height: band.height

            onPaint: {
                const context = getContext("2d")
                const waveWidth = band.width
                const waveHeight = band.height
                const centerY = waveHeight * band.baselineRatio
                const amplitudeY = waveHeight * band.amplitude

                context.clearRect(0, 0, width, height)
                context.fillStyle = band.waveColor
                context.beginPath()
                context.moveTo(0, centerY)
                context.bezierCurveTo(waveWidth * 0.125, centerY - amplitudeY,
                    waveWidth * 0.375, centerY - amplitudeY,
                    waveWidth * 0.5, centerY)
                context.bezierCurveTo(waveWidth * 0.625, centerY + amplitudeY,
                    waveWidth * 0.875, centerY + amplitudeY,
                    waveWidth, centerY)
                context.bezierCurveTo(waveWidth * 1.125, centerY - amplitudeY,
                    waveWidth * 1.375, centerY - amplitudeY,
                    waveWidth * 1.5, centerY)
                context.bezierCurveTo(waveWidth * 1.625, centerY + amplitudeY,
                    waveWidth * 1.875, centerY + amplitudeY,
                    waveWidth * 2, centerY)
                context.lineTo(waveWidth * 2, waveHeight)
                context.lineTo(0, waveHeight)
                context.closePath()
                context.fill()
            }
        }

        onWidthChanged: waveCanvas.requestPaint()
        onHeightChanged: waveCanvas.requestPaint()
        onWaveColorChanged: waveCanvas.requestPaint()
        onBaselineRatioChanged: waveCanvas.requestPaint()
        onAmplitudeChanged: waveCanvas.requestPaint()

        NumberAnimation on phase {
            from: band.reverse ? 1 : 0
            to: band.reverse ? 0 : 1
            duration: band.travelDuration
            loops: Animation.Infinite
            running: band.animationRunning
        }
    }

    WaveBand {
        anchors.fill: parent
        waveColor: Qt.rgba(Theme.secondaryText.r, Theme.secondaryText.g,
            Theme.secondaryText.b, 0.055)
        baselineRatio: 0.47
        amplitude: 0.035
        travelDuration: 26000
        animationRunning: root.visible && root.width > 0 && root.height > 0
        reverse: true
    }

    WaveBand {
        anchors.fill: parent
        waveColor: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.08)
        baselineRatio: 0.59
        amplitude: 0.055
        travelDuration: 19000
        animationRunning: root.visible && root.width > 0 && root.height > 0
    }

    WaveBand {
        anchors.fill: parent
        waveColor: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.12)
        baselineRatio: 0.72
        amplitude: 0.075
        travelDuration: 13000
        animationRunning: root.visible && root.width > 0 && root.height > 0
        reverse: true
    }
}
