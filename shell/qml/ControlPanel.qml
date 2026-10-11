// SPDX-License-Identifier: GPL-3.0-or-later
// Control Panel, in XP's category view: a blue task pane on the left and
// "Pick a category" on the right. A category lists tasks ("Change the
// desktop background") and its Control Panel icons; "Switch to Classic View"
// shows every icon at once. Icons open their applet (Display Properties,
// Sounds and Audio Devices, Mouse, Keyboard, Date and Time, User Accounts,
// Network Connections) through ShellActions.
//
// The body of a labwc-decorated window in a session, of a drawn
// LunaWindowFrame in --preview. `page` selects what is shown: "" for the
// categories, a category key, or "classic".
import QtQuick
import Meadow.Controls
import Meadow.Shell

FocusScope {
    id: cp

    property string page: ""
    readonly property var tk: Theme.tokens.dialogs.controlPanel
    readonly property int pad: Theme.px(tk.contentPadding)

    signal done()

    implicitWidth: Theme.px(tk.width)
    implicitHeight: Theme.px(tk.height)

    Accessible.role: Accessible.Window
    Accessible.name: qsTr("Control Panel")

    // Applets: key -> {label, icon, open()}.
    readonly property var applets: ({
        display: { label: qsTr("Display"), icon: "preferences-desktop-display" },
        sounds: { label: qsTr("Sounds and Audio Devices"), icon: "audio-volume-high" },
        mouse: { label: qsTr("Mouse"), icon: "input-mouse" },
        keyboard: { label: qsTr("Keyboard"), icon: "input-keyboard" },
        datetime: { label: qsTr("Date and Time"), icon: "preferences-system-time" },
        users: { label: qsTr("User Accounts"), icon: "system-users" },
        network: { label: qsTr("Network Connections"), icon: "network-wired" },
    })
    readonly property var categories: [
        { key: "appearance", label: qsTr("Appearance and Themes"), icon: "preferences-desktop-theme",
          tasks: [{ label: qsTr("Change the desktop background"), applet: "display", tab: "desktop" },
                  { label: qsTr("Choose a screen saver"), applet: "display", tab: "screensaver" },
                  { label: qsTr("Change the screen resolution"), applet: "display", tab: "settings" }],
          icons: ["display"] },
        { key: "network", label: qsTr("Network and Internet Connections"), icon: "preferences-system-network",
          tasks: [{ label: qsTr("View network connections"), applet: "network", tab: "" }],
          icons: ["network"] },
        { key: "sounds", label: qsTr("Sounds, Speech, and Audio Devices"), icon: "audio-volume-high",
          tasks: [{ label: qsTr("Adjust the system volume"), applet: "sounds", tab: "volume" },
                  { label: qsTr("Change the sound scheme"), applet: "sounds", tab: "sounds" }],
          icons: ["sounds"] },
        { key: "hardware", label: qsTr("Printers and Other Hardware"), icon: "preferences-desktop-peripherals",
          tasks: [{ label: qsTr("Change the double-click speed"), applet: "mouse", tab: "buttons" },
                  { label: qsTr("Change the keyboard repeat rate"), applet: "keyboard", tab: "speed" }],
          icons: ["mouse", "keyboard"] },
        { key: "users", label: qsTr("User Accounts"), icon: "system-users",
          tasks: [{ label: qsTr("Change my picture"), applet: "users", tab: "picture" }],
          icons: ["users"] },
        { key: "datetime", label: qsTr("Date, Time, Language, and Regional Options"), icon: "preferences-system-time",
          tasks: [{ label: qsTr("Change the date and time"), applet: "datetime", tab: "datetime" },
                  { label: qsTr("Change the time zone"), applet: "datetime", tab: "timezone" },
                  { label: qsTr("Add other languages"), applet: "keyboard", tab: "languages" }],
          icons: ["datetime"] },
    ]
    readonly property var category: categories.find(c => c.key === page) || null

    function openApplet(name, tab) {
        if (name === "display")
            ShellActions.displayPropertiesRequested(tab || "")
        else
            ShellActions.appletRequested(name, tab || "")
    }
    // meadow-control-panel PAGE: an applet name opens that applet; anything
    // else is a page of this window.
    function show(p) {
        if (applets[p] !== undefined) {
            openApplet(p, "")
            page = ""
        } else {
            page = (p === "classic" || categories.some(c => c.key === p)) ? p : ""
        }
    }

    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Backspace && page !== "") {
            page = ""
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        color: cp.tk.contentBackground
    }

    // --- Task pane ----------------------------------------------------------
    component TaskBox: Item {
        id: box
        property string title
        default property alias links: linkColumn.data
        width: parent ? parent.width : 0
        height: header.height + linkColumn.implicitHeight + Theme.px(cp.tk.taskPanePadding)
        GradientBox {
            id: header
            width: parent.width
            height: Theme.px(cp.tk.taskBoxHeaderHeight)
            radius: Theme.px(cp.tk.taskBoxRadius)
            stops: cp.tk.taskBoxHeaderGradient
            Text {
                anchors.verticalCenter: parent.verticalCenter
                x: Theme.px(cp.tk.taskPanePadding)
                text: box.title
                font: Theme.fonts.menuItemBold
                color: cp.tk.taskBoxTitleColor
            }
        }
        Rectangle {
            anchors.top: header.bottom
            width: parent.width
            height: parent.height - header.height
            color: cp.tk.taskBoxBackground
            border.color: cp.tk.taskBoxBorderColor
            border.width: Theme.px(1)
        }
        Column {
            id: linkColumn
            anchors.top: header.bottom
            anchors.topMargin: Theme.px(cp.tk.linkSpacing)
            x: Theme.px(cp.tk.taskPanePadding)
            width: parent.width - x * 2
            spacing: Theme.px(cp.tk.linkSpacing)
        }
    }
    component Link: Text {
        id: link
        property string iconName
        signal activated()
        width: parent ? parent.width : implicitWidth
        leftPadding: iconName ? Theme.px(Theme.tokens.controls.list.iconSize) + Theme.px(cp.tk.linkSpacing) : 0
        wrapMode: Text.Wrap
        font.family: Theme.fonts.ui.family
        font.pixelSize: Theme.fonts.ui.pixelSize
        font.underline: linkMouse.containsMouse
        color: linkMouse.containsMouse ? cp.tk.linkHoverColor : cp.tk.linkColor
        Accessible.role: Accessible.Link
        Accessible.name: text
        Accessible.onPressAction: activated()
        Image {
            visible: link.iconName.length > 0
            width: Theme.px(Theme.tokens.controls.list.iconSize)
            height: width
            sourceSize: Qt.size(width, height)
            source: visible ? "image://icon/" + link.iconName : ""
        }
        MouseArea {
            id: linkMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: link.activated()
        }
    }

    GradientBox {
        id: taskPane
        width: Theme.px(cp.tk.taskPaneWidth)
        height: parent.height
        stops: cp.tk.taskPaneGradient
        Column {
            x: Theme.px(cp.tk.taskPanePadding)
            y: Theme.px(cp.tk.taskPanePadding)
            width: parent.width - x * 2
            spacing: Theme.px(cp.tk.taskPanePadding)
            TaskBox {
                title: qsTr("Control Panel")
                Link {
                    objectName: "switchView"
                    iconName: "view-list-icons"
                    text: cp.page === "classic" ? qsTr("Switch to Category View") : qsTr("Switch to Classic View")
                    onActivated: cp.page = cp.page === "classic" ? "" : "classic"
                }
                Link {
                    visible: cp.category !== null
                    iconName: "go-previous"
                    text: qsTr("Back to categories")
                    onActivated: cp.page = ""
                }
            }
            TaskBox {
                title: qsTr("See Also")
                Link {
                    iconName: "help-browser"
                    text: qsTr("Help and Support")
                    onActivated: Shell.places.activate("help")
                }
            }
        }
    }

    // --- Content ------------------------------------------------------------
    Item {
        id: content
        anchors.left: taskPane.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        GradientBox {
            id: header
            width: parent.width
            height: Theme.px(cp.tk.headerHeight)
            stops: cp.tk.headerGradient
            Row {
                x: cp.pad
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.px(cp.tk.linkSpacing)
                Image {
                    visible: cp.category !== null
                    width: Theme.px(cp.tk.categoryIconSize)
                    height: width
                    sourceSize: Qt.size(width, height)
                    source: cp.category ? "image://icon/" + cp.category.icon : ""
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    font: Theme.fonts.welcomeHint
                    color: cp.tk.titleColor
                    text: cp.page === "classic" ? qsTr("Control Panel")
                        : cp.category ? cp.category.label : qsTr("Pick a category")
                }
            }
        }

        // Category view.
        Grid {
            objectName: "categoryGrid"
            visible: cp.page === ""
            anchors.top: header.bottom
            anchors.topMargin: cp.pad
            x: cp.pad
            columns: 2
            columnSpacing: Theme.px(cp.tk.categorySpacing)
            rowSpacing: Theme.px(cp.tk.categorySpacing)
            Repeater {
                model: cp.categories
                delegate: Item {
                    id: catItem
                    required property var modelData
                    objectName: "category:" + modelData.key
                    width: Theme.px(cp.tk.categoryWidth)
                    height: Theme.px(cp.tk.categoryHeight)
                    Accessible.role: Accessible.Link
                    Accessible.name: modelData.label
                    Accessible.onPressAction: cp.page = modelData.key
                    Image {
                        id: catIcon
                        width: Theme.px(cp.tk.categoryIconSize)
                        height: width
                        sourceSize: Qt.size(width, height)
                        source: "image://icon/" + catItem.modelData.icon
                    }
                    Text {
                        anchors.left: catIcon.right
                        anchors.leftMargin: Theme.px(cp.tk.linkSpacing)
                        anchors.right: parent.right
                        anchors.verticalCenter: catIcon.verticalCenter
                        wrapMode: Text.Wrap
                        text: catItem.modelData.label
                        font.family: Theme.fonts.ui.family
                        font.pixelSize: Theme.fonts.ui.pixelSize
                        font.underline: catMouse.containsMouse
                        color: catMouse.containsMouse ? cp.tk.linkHoverColor : cp.tk.linkColor
                    }
                    MouseArea {
                        id: catMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: cp.page = catItem.modelData.key
                    }
                }
            }
        }

        // One category: tasks, then its icons.
        Column {
            objectName: "categoryPage"
            visible: cp.category !== null
            anchors.top: header.bottom
            anchors.topMargin: cp.pad
            x: cp.pad
            width: parent.width - cp.pad * 2
            spacing: Theme.px(cp.tk.categorySpacing)
            Text {
                text: qsTr("Pick a task...")
                font: Theme.fonts.menuItemBold
                color: cp.tk.titleColor
            }
            Column {
                width: parent.width
                spacing: Theme.px(cp.tk.linkSpacing)
                Repeater {
                    model: cp.category ? cp.category.tasks : []
                    delegate: Link {
                        required property var modelData
                        iconName: "go-next"
                        text: modelData.label
                        onActivated: cp.openApplet(modelData.applet, modelData.tab)
                    }
                }
            }
            Text {
                text: qsTr("or pick a Control Panel icon")
                font: Theme.fonts.menuItemBold
                color: cp.tk.titleColor
            }
            Flow {
                width: parent.width
                spacing: Theme.px(cp.tk.categorySpacing)
                Repeater {
                    model: cp.category ? cp.category.icons : []
                    delegate: AppletIcon { required property string modelData; key: modelData }
                }
            }
        }

        // Classic view: every icon.
        Flow {
            objectName: "classicView"
            visible: cp.page === "classic"
            anchors.top: header.bottom
            anchors.topMargin: cp.pad
            x: cp.pad
            width: parent.width - cp.pad * 2
            spacing: Theme.px(cp.tk.categorySpacing)
            Repeater {
                model: Object.keys(cp.applets)
                delegate: AppletIcon { required property string modelData; key: modelData }
            }
        }
    }

    component AppletIcon: Item {
        id: appletIcon
        property string key
        readonly property var info: cp.applets[key]
        objectName: "applet:" + key
        width: Theme.px(cp.tk.iconViewCell)
        height: Theme.px(cp.tk.iconViewCell)
        Accessible.role: Accessible.Button
        Accessible.name: info.label
        Accessible.onPressAction: cp.openApplet(key, "")
        Image {
            id: appletImage
            anchors.horizontalCenter: parent.horizontalCenter
            width: Theme.px(cp.tk.iconViewIconSize)
            height: width
            sourceSize: Qt.size(width, height)
            source: "image://icon/" + appletIcon.info.icon
        }
        Text {
            anchors.top: appletImage.bottom
            anchors.topMargin: Theme.px(cp.tk.linkSpacing)
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: appletIcon.info.label
            font: Theme.fonts.ui
            color: iconMouse.containsMouse ? cp.tk.linkHoverColor : Theme.tokens.colors.windowText
        }
        MouseArea {
            id: iconMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: cp.openApplet(appletIcon.key, "")
        }
    }
}
