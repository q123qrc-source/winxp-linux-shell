// SPDX-License-Identifier: GPL-3.0-or-later
// The shell's ordinary windows: Display Properties, Control Panel and its
// applets, a desktop item's Properties, and confirmations. In a session
// each is a normal window that labwc decorates; in --preview (previewHost
// set) it is a drawn LunaWindowFrame centered in the preview window.
// One window per kind: opening it again raises it and shows the asked tab.
//
// Listens to ShellActions and the IPC (meadow-shellctl control-panel /
// display-properties) itself.
import QtQuick
import QtQuick.Window
import Meadow.Controls
import Meadow.Shell

Item {
    id: windows

    // --preview: the item frames are drawn in; null in a session.
    property Item previewHost: null

    // kind -> window (Window or LunaWindowFrame) with `loader`.
    property var open: ({})

    readonly property var kinds: ({
        display: { title: qsTr("Display Properties"), component: displayComponent },
        controlPanel: { title: qsTr("Control Panel"), component: controlPanelComponent, resizable: true },
        sounds: { title: qsTr("Sounds and Audio Devices Properties"), component: soundsComponent },
        mouse: { title: qsTr("Mouse Properties"), component: mouseComponent },
        keyboard: { title: qsTr("Keyboard Properties"), component: keyboardComponent },
        datetime: { title: qsTr("Date and Time Properties"), component: dateTimeComponent },
        users: { title: qsTr("User Accounts"), component: usersComponent },
        network: { title: qsTr("Network Connections"), component: networkComponent },
        properties: { title: qsTr("Properties"), component: propertiesComponent },
        confirm: { title: qsTr("Confirm"), component: confirmComponent },
    })

    // Shows `kind`, creating its window on first use; `setup(item)` then
    // prepares the content (load a tab, ...). Returns the content item.
    function show(kind, setup, title) {
        let w = open[kind]
        if (!w) {
            const k = kinds[kind]
            w = (previewHost ? frameComponent : windowComponent).createObject(previewHost ? previewHost : windows, {
                kind: kind, title: title || k.title, resizable: !!k.resizable })
            w.loader.sourceComponent = k.component
            const o = Object.assign({}, open)
            o[kind] = w
            open = o
        } else if (title) {
            w.title = title
        }
        if (setup) {
            if (w.loader.item) {
                setup(w.loader.item)
            } else {
                const loader = w.loader
                const once = function() {
                    loader.loaded.disconnect(once)
                    setup(loader.item)
                    w.present()
                }
                loader.loaded.connect(once)
            }
        }
        w.present()
        return w.loader.item
    }
    function hide(kind) {
        const w = open[kind]
        if (w)
            w.dismiss()
    }

    function openDisplayProperties(tab) { show("display", item => item.load(tab)) }
    function openControlPanel(page) {
        show("controlPanel", item => item.show(page))
    }
    function openApplet(name, tab) {
        if (name === "display")
            openDisplayProperties(tab)
        else if (kinds[name])
            show(name, item => item.load(tab))
    }
    function openItemProperties(details) {
        show("properties", item => item.details = details, qsTr("%1 Properties").arg(details.name))
    }
    function confirm(title, text, onYes) {
        show("confirm", item => { item.text = text; item.onYes = onYes }, title)
    }

    Connections {
        target: ShellActions
        function onDisplayPropertiesRequested(tab) { windows.openDisplayProperties(tab) }
        function onControlPanelRequested(page) { windows.openControlPanel(page) }
        function onAppletRequested(name, tab) { windows.openApplet(name, tab) }
        function onItemPropertiesRequested(details) { windows.openItemProperties(details) }
        function onEmptyTrashRequested() {
            windows.confirm(qsTr("Confirm Multiple File Delete"),
                            qsTr("Are you sure you want to delete all items in the Recycle Bin? This cannot be undone."),
                            () => { Shell.desktop.emptyTrash(); Shell.sounds.play("emptyTrash") })
        }
    }
    Connections {
        target: Shell.ipc
        function onControlPanelRequested(page) { windows.openControlPanel(page) }
        function onDisplayPropertiesRequested(tab) { windows.openDisplayProperties(tab) }
    }

    // --- Contents ---------------------------------------------------------
    Component { id: displayComponent; DisplayProperties {} }
    Component { id: controlPanelComponent; ControlPanel {} }
    Component { id: soundsComponent; SoundsApplet {} }
    Component { id: mouseComponent; MouseApplet {} }
    Component { id: keyboardComponent; KeyboardApplet {} }
    Component { id: dateTimeComponent; DateTimeApplet {} }
    Component { id: usersComponent; UserAccountsApplet {} }
    Component { id: networkComponent; NetworkApplet {} }
    Component { id: propertiesComponent; ItemProperties {} }
    Component { id: confirmComponent; ConfirmDialog {} }

    // --- Session: a real window ------------------------------------------
    Component {
        id: windowComponent
        Window {
            id: win
            property string kind
            property bool resizable: false
            readonly property alias loader: loader
            readonly property int contentW: loader.item ? Math.max(1, loader.item.implicitWidth) : 1
            readonly property int contentH: loader.item ? Math.max(1, loader.item.implicitHeight) : 1
            width: contentW
            height: contentH
            minimumWidth: contentW
            minimumHeight: contentH
            maximumWidth: resizable ? Screen.width : contentW
            maximumHeight: resizable ? Screen.height : contentH
            color: Theme.tokens.dialogs.background
            visible: false

            function present() {
                visible = true
                requestActivate()
                if (loader.item)
                    loader.item.forceActiveFocus()
            }
            function dismiss() { visible = false }

            Loader {
                id: loader
                anchors.fill: parent
                focus: true
            }
            Connections {
                target: loader.item
                ignoreUnknownSignals: true
                function onDone() { win.dismiss() }
            }
        }
    }

    // --- Preview: a drawn frame ------------------------------------------
    Component {
        id: frameComponent
        LunaWindowFrame {
            id: frame
            property string kind
            property bool resizable: false
            readonly property alias loader: loader
            objectName: "window:" + kind
            anchors.centerIn: parent
            z: 1
            visible: false
            showMinimize: false
            showMaximize: false
            onCloseClicked: dismiss()

            function present() {
                visible = true
                // The latest window on top.
                for (const k in windows.open)
                    windows.open[k].z = 1
                z = 2
                if (loader.item)
                    loader.item.forceActiveFocus()
            }
            function dismiss() { visible = false }

            Loader {
                id: loader
                width: item ? item.implicitWidth : 0
                height: item ? item.implicitHeight : 0
                focus: true
            }
            Connections {
                target: loader.item
                ignoreUnknownSignals: true
                function onDone() { frame.dismiss() }
            }
        }
    }
}
