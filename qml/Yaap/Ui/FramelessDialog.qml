import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import Yaap.ModApi 1.0

Window {
    id: root

    property bool modal: true
    property int standardButtons: Dialog.Close
    property string settingsKey
    property int defaultWidth: 640
    property int defaultHeight: 480
    property alias contentItem: contentControl.contentItem
    property bool completingAction: false
    property bool positioned: false
    property bool logicalModalActive: false
    property var logicalModalParent: null

    signal accepted()
    signal rejected()
    signal opened()

    width: defaultWidth
    height: defaultHeight
    minimumWidth: 360
    minimumHeight: 240
    visible: false
    color: "transparent"
    flags: Qt.Dialog | Qt.FramelessWindowHint
    // PROTOTYPE: Native Qt.ApplicationModal leaves the owning HWND disabled
    // after persistent QML windows are hidden on Windows. Keep the native
    // windows non-modal and block the parent's QML content explicitly instead.
    // A shared dialog/window manager can replace this local lifecycle later.
    modality: Qt.NonModal

    function acquireLogicalModality() {
        if (!modal || logicalModalActive || !transientParent)
            return

        const parentWindow = transientParent
        if (typeof parentWindow.framelessModalDepth !== "number")
            return

        logicalModalParent = parentWindow
        logicalModalActive = true
        parentWindow.framelessModalDepth += 1
        parentWindow.contentItem.enabled = false
    }

    function releaseLogicalModality() {
        if (!logicalModalActive)
            return

        const parentWindow = logicalModalParent
        logicalModalActive = false
        logicalModalParent = null
        if (!parentWindow
                || typeof parentWindow.framelessModalDepth !== "number")
            return

        parentWindow.framelessModalDepth = Math.max(
            0, parentWindow.framelessModalDepth - 1)
        parentWindow.contentItem.enabled = parentWindow.framelessModalDepth === 0
    }

    function open() {
        if (!positioned && transientParent) {
            x = Math.round(transientParent.x + (transientParent.width - width) / 2)
            y = Math.round(transientParent.y + (transientParent.height - height) / 2)
            positioned = true
        }
        acquireLogicalModality()
        show()
        raise()
        requestActivate()
        opened()
    }

    function finish(acceptedResult) {
        if (!visible || completingAction)
            return

        completingAction = true
        hide()
        releaseLogicalModality()
        if (acceptedResult)
            accepted()
        else
            rejected()

        const parentWindow = transientParent
        if (parentWindow) {
            Qt.callLater(function() {
                parentWindow.raise()
                parentWindow.requestActivate()
            })
        }
        completingAction = false
    }

    function accept() { finish(true) }

    function reject() { finish(false) }

    onClosing: function(event) {
        const parentWindow = transientParent
        if (!visible || (parentWindow && parentWindow.applicationClosing)) {
            releaseLogicalModality()
            event.accepted = true
            return
        }

        event.accepted = false
        finish(false)
    }

    onVisibleChanged: {
        if (!visible)
            releaseLogicalModality()
    }

    Settings {
        category: "dialogs/" + root.settingsKey
        property alias width: root.width
        property alias height: root.height
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.surface
        radius: Theme.cornerRadius
        border.color: Theme.accent
        border.width: 1
    }

    Item {
        id: titleBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 42
        z: 2

        Label {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.right: dialogCloseButton.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.primaryText
            font.bold: true
            elide: Text.ElideRight
        }

        MouseArea {
            anchors.left: parent.left
            anchors.right: dialogCloseButton.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            acceptedButtons: Qt.LeftButton
            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.ArrowCursor
            onPressed: root.startSystemMove()
        }

        ToolButton {
            id: dialogCloseButton

            anchors.right: parent.right
            anchors.top: parent.top
            width: 44
            height: parent.height
            text: "×"
            flat: true
            font.pixelSize: 20
            Accessible.name: "Close " + root.title
            onClicked: root.reject()

            background: Rectangle {
                color: dialogCloseButton.down
                    ? Qt.darker(Theme.error, 1.15)
                    : dialogCloseButton.hovered ? Theme.error : "transparent"
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: titleBar.bottom
        height: 1
        color: Theme.accent
        opacity: 0.35
    }

    Control {
        id: contentControl

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: titleBar.bottom
        anchors.bottom: dialogFooter.top
        leftPadding: 16
        rightPadding: 16
        topPadding: 14
        bottomPadding: 14
        background: null
    }

    RowLayout {
        id: dialogFooter

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: root.standardButtons === 0 ? 0 : 52
        spacing: 8

        Item { Layout.fillWidth: true }
        Button {
            visible: (root.standardButtons & Dialog.Cancel) !== 0
            text: "Cancel"
            onClicked: root.reject()
        }
        Button {
            visible: (root.standardButtons & Dialog.Close) !== 0
            text: "Close"
            onClicked: root.reject()
        }
        Button {
            visible: (root.standardButtons & Dialog.Ok) !== 0
            text: "OK"
            onClicked: root.accept()
        }
        Item { Layout.preferredWidth: 14 }
    }

    component ResizeHandle: MouseArea {
        required property int edges

        acceptedButtons: Qt.LeftButton
        z: 20
        onPressed: root.startSystemResize(edges)
    }

    ResizeHandle {
        edges: Qt.LeftEdge
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 6
        cursorShape: Qt.SizeHorCursor
    }
    ResizeHandle {
        edges: Qt.RightEdge
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 6
        cursorShape: Qt.SizeHorCursor
    }
    ResizeHandle {
        edges: Qt.TopEdge
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 6
        cursorShape: Qt.SizeVerCursor
    }
    ResizeHandle {
        edges: Qt.BottomEdge
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 6
        cursorShape: Qt.SizeVerCursor
    }
    ResizeHandle {
        edges: Qt.LeftEdge | Qt.TopEdge
        anchors.left: parent.left
        anchors.top: parent.top
        width: 10
        height: 10
        cursorShape: Qt.SizeFDiagCursor
    }
    ResizeHandle {
        edges: Qt.RightEdge | Qt.TopEdge
        anchors.right: parent.right
        anchors.top: parent.top
        width: 10
        height: 10
        cursorShape: Qt.SizeBDiagCursor
    }
    ResizeHandle {
        edges: Qt.LeftEdge | Qt.BottomEdge
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: 10
        height: 10
        cursorShape: Qt.SizeBDiagCursor
    }
    ResizeHandle {
        edges: Qt.RightEdge | Qt.BottomEdge
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 10
        height: 10
        cursorShape: Qt.SizeFDiagCursor
    }
}
