// SPDX-License-Identifier: GPL-3.0-or-later
// Window chrome drawn by the shell for its own windows (Run, Display
// Properties, Control Panel, dialogs): rounded blue title bar with a bold
// shadowed title, minimize / maximize / close buttons and a bevelled border.
// Third-party windows get the same look from labwc's Meadow-Luna theme.
import QtQuick
import QtQuick.Window
import Meadow.Controls

Item {
    id: root

    property string title
    property bool active: true
    // Optional 16px window icon.
    property url icon
    property bool showMinimize: true
    property bool showMaximize: true
    property bool showClose: true
    property bool maximized: false
    // Size of the client area; the frame adds title bar and borders around it.
    property real contentWidth: contentArea.childrenRect.width
    property real contentHeight: contentArea.childrenRect.height
    // Forced caption button states for the gallery and screenshot tests,
    // e.g. ({ close: "hover", maximize: "pressed" }).
    property var previewButtonStates: ({})

    default property alias content: contentArea.data

    signal minimizeClicked()
    signal maximizeClicked()
    signal closeClicked()

    readonly property var tk: Theme.tokens.window
    readonly property int borderWidth: Theme.px(tk.border.width)
    readonly property int titleHeight: Theme.px(tk.titlebar.height)
    readonly property int cornerRadius: maximized ? 0 : Theme.px(tk.titlebar.radiusTop)

    implicitWidth: contentWidth + borderWidth * 2
    implicitHeight: titleHeight + contentHeight + borderWidth

    Accessible.role: Accessible.Window
    Accessible.name: title

    component CaptionButton: GradientBox {
        id: button

        property int glyph
        property bool isClose: false
        property string key
        property string accessibleName
        signal activated()

        readonly property string forced: root.previewButtonStates[key] || ""
        readonly property var set: isClose ? root.tk.buttons.close : root.tk.buttons.normal

        width: Theme.px(root.tk.buttons.size)
        height: width
        radius: Theme.px(root.tk.buttons.radius)
        borderWidth: Theme.px(1)
        borderColor: root.tk.buttons.borderColor
        stops: !root.active ? root.tk.buttons.inactive
             : (mouse.pressed || forced === "pressed") ? set.pressed
             : (mouse.containsMouse || forced === "hover") ? set.hover
             : set.normal

        LunaGlyph {
            anchors.fill: parent
            anchors.margins: Theme.px(root.tk.buttons.glyphInset)
            glyph: button.glyph
            color: button.isClose ? root.tk.buttons.closeGlyphColor : root.tk.buttons.glyphColor
        }
        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: button.activated()
        }

        Accessible.role: Accessible.Button
        Accessible.name: accessibleName
        Accessible.onPressAction: activated()
    }

    // Border: frame color with a darker 1px outline (bevel shadow) and a
    // lighter inner line (bevel highlight).
    GradientBox {
        anchors.fill: parent
        color: root.active ? root.tk.border.activeColor : root.tk.border.inactiveColor
        borderWidth: Theme.px(1)
        borderColor: root.active ? root.tk.border.activeShadow : root.tk.border.inactiveShadow
        topLeftRadius: root.cornerRadius
        topRightRadius: root.cornerRadius
        bottomLeftRadius: 0
        bottomRightRadius: 0
    }

    GradientBox {
        anchors.fill: parent
        anchors.margins: Theme.px(1)
        anchors.topMargin: root.titleHeight
        borderWidth: Theme.px(1)
        borderColor: root.active ? root.tk.border.activeHighlight : root.tk.border.inactiveHighlight
    }

    GradientBox {
        id: titleBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.titleHeight
        stops: root.active ? root.tk.titlebar.gradient.active : root.tk.titlebar.gradient.inactive
        topLeftRadius: root.cornerRadius
        topRightRadius: root.cornerRadius
        bottomLeftRadius: 0
        bottomRightRadius: 0

        Image {
            id: iconImage
            x: Theme.px(root.tk.titlebar.paddingLeft)
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.px(root.tk.titlebar.iconSize)
            height: width
            sourceSize: Qt.size(width, height)
            source: root.icon
            visible: root.icon.toString().length > 0
        }

        ShadowText {
            anchors.left: iconImage.visible ? iconImage.right : parent.left
            anchors.leftMargin: iconImage.visible ? Theme.px(root.tk.titlebar.textSpacing) : Theme.px(root.tk.titlebar.paddingLeft)
            anchors.right: buttons.left
            anchors.rightMargin: Theme.px(root.tk.titlebar.textSpacing)
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            font: Theme.fonts.title
            color: root.active ? root.tk.titlebar.textColor : root.tk.titlebar.inactiveTextColor
            shadowColor: root.active ? root.tk.titlebar.textShadowColor : "transparent"
            shadowOffset: Theme.px(root.tk.titlebar.textShadowOffset)
            elide: Text.ElideRight
        }

        // Double-click maximizes; dragging moves the window when it is a real
        // top-level surface.
        TapHandler {
            onDoubleTapped: if (root.showMaximize) root.maximizeClicked()
        }
        DragHandler {
            target: null
            onActiveChanged: if (active && root.Window.window) root.Window.window.startSystemMove()
        }

        Row {
            id: buttons
            anchors.right: parent.right
            anchors.rightMargin: Theme.px(root.tk.buttons.marginRight)
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(root.tk.buttons.spacing)

            CaptionButton {
                key: "minimize"
                visible: root.showMinimize
                glyph: LunaGlyph.Minimize
                accessibleName: qsTr("Minimize")
                onActivated: root.minimizeClicked()
            }
            CaptionButton {
                key: "maximize"
                visible: root.showMaximize
                glyph: root.maximized ? LunaGlyph.Restore : LunaGlyph.Maximize
                accessibleName: root.maximized ? qsTr("Restore") : qsTr("Maximize")
                onActivated: root.maximizeClicked()
            }
            CaptionButton {
                key: "close"
                isClose: true
                visible: root.showClose
                glyph: LunaGlyph.Close
                accessibleName: qsTr("Close")
                onActivated: root.closeClicked()
            }
        }
    }

    GradientBox {
        id: clientBackground
        x: root.borderWidth
        y: root.titleHeight
        width: root.width - root.borderWidth * 2
        height: root.height - root.titleHeight - root.borderWidth
        color: Theme.tokens.colors.windowBackground
    }

    Item {
        id: contentArea
        anchors.fill: clientBackground
        clip: true
    }
}
