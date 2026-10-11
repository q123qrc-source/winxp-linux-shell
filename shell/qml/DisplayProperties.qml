// SPDX-License-Identifier: GPL-3.0-or-later
// "Display Properties": Themes, Desktop, Screen Saver, Appearance and
// Settings tabs. Changes are pending until Apply or OK; Cancel discards
// them. Saved to shell.conf ([Appearance], [Desktop], [ScreenSaver]); the
// color scheme applies live (Theme.loadScheme, and the window decorations
// through LabwcConfig). Screen resolution goes through Shell.outputs and is
// confirmed with a 15-second "Keep these settings?" countdown that reverts
// on its own.
//
// The body of a labwc-decorated window in a session, of a drawn
// LunaWindowFrame in --preview.
import QtQuick
import Meadow.Controls
import Meadow.Shell

FocusScope {
    id: dp

    readonly property var tk: Theme.tokens.dialogs.displayProperties
    readonly property int pad: Theme.px(Theme.tokens.dialogs.padding)
    readonly property int gap: Theme.px(tk.fieldSpacing)
    readonly property color paneColor: Theme.tokens.controls.tab.paneBackground
    readonly property var tabKeys: ["themes", "desktop", "screensaver", "appearance", "settings"]

    signal done()

    implicitWidth: Theme.px(tk.width)
    implicitHeight: Theme.px(tk.height)

    // --- Pending values ---------------------------------------------------

    property string scheme
    property string fontSize
    property string wallpaper
    property string wallpaperMode
    property string bgColor
    property bool showComputer
    property bool showDocuments
    property bool showTrash
    property string saver
    property int saverMinutes
    property bool passwordProtect
    property bool lockOnSuspend
    // Settings tab: chosen size/scale for the first output.
    property int outWidth
    property int outHeight
    property real outScale
    property var wallpapers: []
    // Countdown after a resolution change, 0 when not asking.
    property int keepSeconds: 0
    property string outputError: ""

    readonly property var output: Shell.outputs.outputs.length > 0 ? Shell.outputs.outputs[0] : null

    function settingString(key, fallback) { return Shell.settings.stringValue(key, fallback) }

    function load(tab) {
        scheme = settingString("Appearance/scheme", Theme.schemeId)
        fontSize = settingString("Appearance/fontSize", "normal")
        wallpaper = settingString("Desktop/wallpaper", Theme.tokens.desktop.wallpaper)
        wallpaperMode = settingString("Desktop/wallpaperMode", Theme.tokens.desktop.wallpaperMode)
        bgColor = settingString("Desktop/color", "")
        showComputer = Shell.settings.boolSetting("Desktop/showMyComputer", true)
        showDocuments = Shell.settings.boolSetting("Desktop/showMyDocuments", true)
        showTrash = Shell.settings.boolSetting("Desktop/showRecycleBin", true)
        saver = settingString("ScreenSaver/saver", "logo")
        saverMinutes = Shell.settings.intSetting("ScreenSaver/timeoutMin", Theme.tokens.screenSaver.defaultMinutes)
        passwordProtect = Shell.settings.boolSetting("ScreenSaver/passwordProtect", true)
        lockOnSuspend = Shell.settings.boolSetting("ScreenSaver/lockOnSuspend", true)
        resetOutput()
        wallpapers = Shell.desktop.wallpaperChoices()
        // After the list has its new model (a model reset moves its index).
        Qt.callLater(() => wallpaperList.currentIndex = desktopPage.current)
        keepSeconds = 0
        outputError = ""
        const i = tabKeys.indexOf(tab)
        tabs.currentIndex = i >= 0 ? i : 0
    }
    function resetOutput() {
        outWidth = output ? output.width : 0
        outHeight = output ? output.height : 0
        outScale = output ? output.scale : 1
    }

    readonly property bool outputDirty: !!output && (outWidth !== output.width || outHeight !== output.height
                                                     || Math.abs(outScale - output.scale) > Number.EPSILON)
    readonly property bool dirty: {
        Shell.settings.revision
        return scheme !== settingString("Appearance/scheme", Theme.schemeId)
            || fontSize !== settingString("Appearance/fontSize", "normal")
            || wallpaper !== settingString("Desktop/wallpaper", Theme.tokens.desktop.wallpaper)
            || wallpaperMode !== settingString("Desktop/wallpaperMode", Theme.tokens.desktop.wallpaperMode)
            || bgColor !== settingString("Desktop/color", "")
            || showComputer !== Shell.settings.boolSetting("Desktop/showMyComputer", true)
            || showDocuments !== Shell.settings.boolSetting("Desktop/showMyDocuments", true)
            || showTrash !== Shell.settings.boolSetting("Desktop/showRecycleBin", true)
            || saver !== settingString("ScreenSaver/saver", "logo")
            || saverMinutes !== Shell.settings.intSetting("ScreenSaver/timeoutMin", Theme.tokens.screenSaver.defaultMinutes)
            || passwordProtect !== Shell.settings.boolSetting("ScreenSaver/passwordProtect", true)
            || lockOnSuspend !== Shell.settings.boolSetting("ScreenSaver/lockOnSuspend", true)
            || outputDirty
    }

    function apply() {
        Shell.settings.set("Desktop/wallpaper", wallpaper)
        Shell.settings.set("Desktop/wallpaperMode", wallpaperMode)
        Shell.settings.set("Desktop/color", bgColor)
        Shell.settings.set("Desktop/showMyComputer", showComputer)
        Shell.settings.set("Desktop/showMyDocuments", showDocuments)
        Shell.settings.set("Desktop/showRecycleBin", showTrash)
        Shell.settings.set("ScreenSaver/saver", saver)
        Shell.settings.set("ScreenSaver/timeoutMin", saverMinutes)
        Shell.settings.set("ScreenSaver/passwordProtect", passwordProtect)
        Shell.settings.set("ScreenSaver/lockOnSuspend", lockOnSuspend)
        Shell.settings.set("Appearance/fontSize", fontSize)
        if (scheme !== Theme.schemeId)
            Theme.loadScheme(scheme)
        Shell.settings.set("Appearance/scheme", scheme)
        if (outputDirty) {
            outputError = ""
            Shell.outputs.apply(output.name, outWidth, outHeight, 0, outScale)
        }
    }
    function accept() {
        const waitForOutput = outputDirty
        apply()
        if (!waitForOutput)
            done()
        else
            closeAfterKeep = true
    }
    property bool closeAfterKeep: false

    Connections {
        target: Shell.outputs
        function onApplied(ok, message) {
            if (!ok) {
                dp.outputError = message
                dp.resetOutput()
                dp.closeAfterKeep = false
                return
            }
            if (Shell.outputs.canRevert)
                dp.keepSeconds = dp.tk.revertSeconds
            else
                dp.resetOutput()
        }
    }
    Timer {
        running: dp.keepSeconds > 0
        interval: dp.tk.countdownTickMs
        repeat: true
        onTriggered: {
            dp.keepSeconds -= 1
            if (dp.keepSeconds === 0)
                dp.revertOutput()
        }
    }
    function keepOutput() {
        keepSeconds = 0
        Shell.outputs.keep()
        resetOutput()
        if (closeAfterKeep)
            done()
    }
    function revertOutput() {
        keepSeconds = 0
        closeAfterKeep = false
        Shell.outputs.revert()
        resetOutput()
    }

    Keys.onEscapePressed: if (keepSeconds === 0) done()

    Rectangle {
        anchors.fill: parent
        color: Theme.tokens.dialogs.background
    }

    // --- Shared bits ------------------------------------------------------

    // The pending wallpaper over the pending color, as on the desktop.
    component DesktopSample: Item {
        id: sample
        property real iconScale: 1
        Rectangle {
            anchors.fill: parent
            color: dp.bgColor.length > 0 ? dp.bgColor : Theme.tokens.desktop.background
        }
        Image {
            anchors.fill: parent
            visible: dp.wallpaper.length > 0
            source: dp.wallpaper.length === 0 ? "" : (dp.wallpaper.startsWith("/") ? "file://" + dp.wallpaper : Theme.asset(dp.wallpaper))
            sourceSize: Qt.size(width, height)
            fillMode: dp.wallpaperMode === "tile" ? Image.Tile : dp.wallpaperMode === "center" ? Image.Pad : Image.PreserveAspectCrop
            asynchronous: !Shell.preview
        }
    }

    LunaTabView {
        id: tabs
        objectName: "displayTabs"
        x: dp.pad
        y: dp.pad
        width: dp.width - dp.pad * 2
        height: buttons.y - dp.pad * 2
        focus: true

        // --- Themes -------------------------------------------------------
        Item {
            id: themesPage
            objectName: "page:themes"
            property string title: qsTr("Themes")
            readonly property var choices: [
                { label: qsTr("Meadow"), scheme: "luna-blue", wallpaper: "meadow-hills.svg", color: "" },
                { label: qsTr("Meadow (Olive Green)"), scheme: "luna-olive", wallpaper: "meadow-hills.svg", color: "" },
                { label: qsTr("Meadow (Silver)"), scheme: "luna-silver", wallpaper: "meadow-hills.svg", color: "" },
                { label: qsTr("Meadow Classic"), scheme: "luna-blue", wallpaper: "", color: Theme.tokens.desktop.background },
            ]
            function currentChoice() {
                for (let i = 0; i < choices.length; ++i) {
                    const c = choices[i]
                    if (c.scheme === dp.scheme && c.wallpaper === dp.wallpaper && (c.color === "" || c.color === dp.bgColor))
                        return i
                }
                return -1
            }
            Column {
                width: parent.width
                spacing: dp.gap
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    font: Theme.fonts.ui
                    text: qsTr("A theme is a background plus a color scheme and other elements that give your desktop a unified look.")
                }
                Text { text: qsTr("Theme:"); font: Theme.fonts.ui }
                LunaComboBox {
                    objectName: "themeCombo"
                    width: parent.width
                    model: themesPage.choices.map(c => c.label).concat(themesPage.currentChoice() < 0 ? [qsTr("Modified Theme")] : [])
                    currentIndex: themesPage.currentChoice() >= 0 ? themesPage.currentChoice() : model.length - 1
                    onActivated: (index) => {
                        const c = themesPage.choices[index]
                        if (!c)
                            return
                        dp.scheme = c.scheme
                        dp.wallpaper = c.wallpaper
                        dp.bgColor = c.color
                    }
                }
                Text { text: qsTr("Sample:"); font: Theme.fonts.ui }
                MonitorPreview {
                    anchors.horizontalCenter: parent.horizontalCenter
                    DesktopSample {
                        anchors.fill: parent
                        LunaWindowFrame {
                            x: Theme.px(Theme.tokens.dialogs.padding)
                            y: Theme.px(Theme.tokens.dialogs.padding)
                            width: Theme.px(dp.tk.sampleWindowWidth)
                            height: Theme.px(dp.tk.sampleWindowHeight)
                            title: qsTr("Active Window")
                            showMinimize: false
                        }
                    }
                }
            }
        }

        // --- Desktop ------------------------------------------------------
        Item {
            id: desktopPage
            objectName: "page:desktop"
            property string title: qsTr("Desktop")
            readonly property int current: {
                for (let i = 0; i < dp.wallpapers.length; ++i) {
                    if (dp.wallpapers[i].value === dp.wallpaper)
                        return i
                }
                return -1
            }

            MonitorPreview {
                id: deskMonitor
                anchors.horizontalCenter: parent.horizontalCenter
                DesktopSample { anchors.fill: parent }
            }
            Text {
                id: bgLabel
                y: deskMonitor.height + Theme.px(dp.tk.monitorSpacing)
                text: qsTr("Background:")
                font: Theme.fonts.ui
            }
            LunaListView {
                id: wallpaperList
                objectName: "wallpaperList"
                anchors.top: bgLabel.bottom
                anchors.topMargin: dp.gap
                anchors.left: parent.left
                anchors.right: deskButtons.left
                anchors.rightMargin: dp.pad
                anchors.bottom: customize.top
                anchors.bottomMargin: dp.gap
                model: dp.wallpapers.map(w => ({ label: w.label, iconName: w.value === "" ? "" : "image-x-generic" }))
                // Only the user's choice (a model reset also moves the index).
                onCurrentIndexChanged: if (activeFocus && currentIndex >= 0 && currentIndex < dp.wallpapers.length)
                                           dp.wallpaper = dp.wallpapers[currentIndex].value
            }
            Column {
                id: deskButtons
                anchors.top: wallpaperList.top
                anchors.right: parent.right
                spacing: dp.gap
                LunaButton {
                    objectName: "browseWallpaper"
                    text: qsTr("Browse…")
                    onClicked: Shell.run.browse(qsTr("Browse"), "wallpaper")
                }
                Text { text: qsTr("Position:"); font: Theme.fonts.ui }
                LunaComboBox {
                    objectName: "positionCombo"
                    width: Theme.tokens.controls.button.minWidth * Theme.scale
                    readonly property var keys: ["center", "tile", "stretch"]
                    model: [qsTr("Center"), qsTr("Tile"), qsTr("Stretch")]
                    currentIndex: Math.max(0, keys.indexOf(dp.wallpaperMode))
                    onActivated: (index) => dp.wallpaperMode = keys[index]
                }
                Text { text: qsTr("Color:"); font: Theme.fonts.ui }
                LunaButton {
                    id: colorButton
                    objectName: "colorButton"
                    implicitWidth: Theme.px(Theme.tokens.controls.button.minWidth) / 2
                    onClicked: colorPopup.visible = !colorPopup.visible
                    Rectangle {
                        anchors.centerIn: parent
                        width: Theme.px(dp.tk.colorSwatchSize) - Theme.px(Theme.tokens.dialogs.buttonSpacing)
                        height: Theme.px(Theme.tokens.controls.checkBox.size)
                        color: dp.bgColor.length > 0 ? dp.bgColor : Theme.tokens.desktop.background
                        border.color: Theme.tokens.colors.windowText
                        border.width: Theme.px(1)
                    }
                }
            }
            LunaButton {
                id: customize
                objectName: "customizeDesktop"
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                text: qsTr("Customize Desktop…")
                onClicked: customizePopup.visible = true
            }

            // Basic colors, like XP's color picker.
            Rectangle {
                id: colorPopup
                visible: false
                z: 2
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: swatches.width + dp.pad * 2
                height: swatches.height + dp.pad * 2
                color: Theme.tokens.dialogs.background
                border.color: Theme.tokens.controls.menu.borderColor
                border.width: Theme.px(1)
                Grid {
                    id: swatches
                    x: dp.pad
                    y: dp.pad
                    columns: dp.tk.colorColumns
                    spacing: Theme.px(2)
                    Repeater {
                        model: Theme.tokens.dialogs.displayProperties.basicColors
                        delegate: Rectangle {
                            required property string modelData
                            width: Theme.px(dp.tk.colorSwatchSize)
                            height: width
                            color: modelData
                            border.color: modelData === dp.bgColor ? Theme.tokens.colors.selection : Theme.tokens.colors.disabledText
                            border.width: modelData === dp.bgColor ? Theme.px(2) : Theme.px(1)
                            Accessible.role: Accessible.Button
                            Accessible.name: modelData
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    dp.bgColor = parent.modelData
                                    colorPopup.visible = false
                                }
                            }
                        }
                    }
                }
            }

            // "Desktop Items": which special icons the desktop shows.
            Rectangle {
                id: customizePopup
                visible: false
                z: 2
                anchors.centerIn: parent
                width: itemsBox.implicitWidth + dp.pad * 2
                height: itemsColumn.implicitHeight + dp.pad * 2
                color: Theme.tokens.dialogs.background
                border.color: Theme.tokens.controls.menu.borderColor
                border.width: Theme.px(1)
                Column {
                    id: itemsColumn
                    x: dp.pad
                    y: dp.pad
                    spacing: dp.gap
                    LunaGroupBox {
                        id: itemsBox
                        title: qsTr("Desktop icons")
                        Column {
                            spacing: dp.gap
                            LunaCheckBox { objectName: "option:showMyDocuments"; text: qsTr("My Documents"); checked: dp.showDocuments; onToggled: dp.showDocuments = checked }
                            LunaCheckBox { objectName: "option:showMyComputer"; text: qsTr("My Computer"); checked: dp.showComputer; onToggled: dp.showComputer = checked }
                            LunaCheckBox { objectName: "option:showRecycleBin"; text: qsTr("Recycle Bin"); checked: dp.showTrash; onToggled: dp.showTrash = checked }
                        }
                    }
                    LunaButton {
                        anchors.right: parent.right
                        text: qsTr("OK")
                        isDefault: true
                        onClicked: customizePopup.visible = false
                    }
                }
            }

            Connections {
                target: Shell.run
                function onBrowsed(path, tag) {
                    if (tag !== "wallpaper")
                        return
                    if (!Shell.desktop.isImage(path)) {
                        dp.outputError = qsTr("\"%1\" is not a picture Meadow can show.").arg(path)
                        return
                    }
                    const list = dp.wallpapers.slice()
                    if (!list.some(w => w.value === path))
                        list.push({ label: path.substring(path.lastIndexOf("/") + 1), value: path })
                    dp.wallpapers = list
                    dp.wallpaper = path
                    Qt.callLater(() => wallpaperList.currentIndex = desktopPage.current)
                }
            }
        }

        // --- Screen Saver -------------------------------------------------
        Item {
            id: saverPage
            objectName: "page:screensaver"
            property string title: qsTr("Screen Saver")
            readonly property var keys: ["none", "blank", "logo"]

            MonitorPreview {
                id: saverMonitor
                anchors.horizontalCenter: parent.horizontalCenter
                ScreenSaver {
                    anchors.fill: parent
                    mode: dp.saver
                    preview: true
                    visible: dp.saver !== "none"
                }
                DesktopSample {
                    anchors.fill: parent
                    visible: dp.saver === "none"
                }
            }
            LunaGroupBox {
                id: saverBox
                anchors.top: saverMonitor.bottom
                anchors.topMargin: Theme.px(dp.tk.monitorSpacing)
                width: parent.width
                title: qsTr("Screen saver")
                titleBackground: dp.paneColor
                Column {
                    spacing: dp.gap
                    Row {
                        spacing: dp.gap
                        LunaComboBox {
                            objectName: "saverCombo"
                            model: [qsTr("(None)"), qsTr("Blank"), qsTr("Meadow Logo")]
                            currentIndex: Math.max(0, saverPage.keys.indexOf(dp.saver))
                            onActivated: (index) => dp.saver = saverPage.keys[index]
                        }
                        LunaButton {
                            objectName: "saverPreview"
                            text: qsTr("Preview")
                            enabled: dp.saver !== "none"
                            onClicked: ShellActions.screenSaverPreviewRequested(dp.saver)
                        }
                    }
                    Row {
                        spacing: dp.gap
                        enabled: dp.saver !== "none"
                        Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("Wait:"); font: Theme.fonts.ui }
                        LunaSpinBox {
                            objectName: "saverMinutes"
                            from: 1
                            to: Theme.tokens.screenSaver.maxMinutes
                            value: dp.saverMinutes
                            onValueModified: dp.saverMinutes = value
                        }
                        Text { anchors.verticalCenter: parent.verticalCenter; text: qsTr("minutes"); font: Theme.fonts.ui }
                    }
                    LunaCheckBox {
                        objectName: "option:passwordProtect"
                        enabled: dp.saver !== "none"
                        text: qsTr("On resume, password protect")
                        checked: dp.passwordProtect
                        onToggled: dp.passwordProtect = checked
                    }
                }
            }
            LunaGroupBox {
                anchors.top: saverBox.bottom
                anchors.topMargin: dp.gap
                width: parent.width
                title: qsTr("Sleep")
                titleBackground: dp.paneColor
                LunaCheckBox {
                    objectName: "option:lockOnSuspend"
                    text: qsTr("Lock the computer before it goes to sleep")
                    checked: dp.lockOnSuspend
                    onToggled: dp.lockOnSuspend = checked
                }
            }
            Text {
                anchors.bottom: parent.bottom
                width: parent.width
                visible: !Shell.idle.available
                wrapMode: Text.Wrap
                font: Theme.fonts.small
                color: Theme.tokens.colors.disabledText
                text: qsTr("The screen saver needs a compositor that reports idle time (ext-idle-notify-v1).")
            }
        }

        // --- Appearance ---------------------------------------------------
        Item {
            id: appearancePage
            objectName: "page:appearance"
            property string title: qsTr("Appearance")
            readonly property var schemes: ["luna-blue", "luna-olive", "luna-silver"]

            Rectangle {
                id: appearanceSample
                width: parent.width
                height: Theme.px(dp.tk.sampleWindowHeight) * 2
                color: Theme.tokens.desktop.background
                border.color: Theme.tokens.controls.textField.borderColor
                border.width: Theme.px(1)
                LunaWindowFrame {
                    x: Theme.px(Theme.tokens.dialogs.padding)
                    y: Theme.px(Theme.tokens.dialogs.padding)
                    width: Theme.px(dp.tk.sampleWindowWidth)
                    height: Theme.px(dp.tk.sampleWindowHeight)
                    title: qsTr("Inactive Window")
                    active: false
                    showMinimize: false
                }
                LunaWindowFrame {
                    x: Theme.px(Theme.tokens.dialogs.padding) * 3 // theme-ok: overlapping sample windows
                    y: Theme.px(dp.tk.sampleWindowHeight) / 2 + Theme.px(Theme.tokens.dialogs.padding)
                    width: Theme.px(dp.tk.sampleWindowWidth) * 3 / 2 // theme-ok: wider active sample
                    height: Theme.px(dp.tk.sampleWindowHeight)
                    title: qsTr("Active Window")
                    LunaButton { x: Theme.px(Theme.tokens.dialogs.padding); y: Theme.px(Theme.tokens.dialogs.padding); text: qsTr("OK"); isDefault: true }
                }
            }
            Grid {
                anchors.top: appearanceSample.bottom
                anchors.topMargin: dp.pad
                columns: 2
                columnSpacing: dp.pad
                rowSpacing: dp.gap
                verticalItemAlignment: Grid.AlignVCenter
                Text { text: qsTr("Windows and buttons:"); font: Theme.fonts.ui }
                LunaComboBox { model: [qsTr("Meadow style")]; enabled: false }
                Text { text: qsTr("Color scheme:"); font: Theme.fonts.ui }
                LunaComboBox {
                    objectName: "schemeCombo"
                    model: [qsTr("Default (blue)"), qsTr("Olive Green"), qsTr("Silver")]
                    currentIndex: Math.max(0, appearancePage.schemes.indexOf(dp.scheme))
                    onActivated: (index) => dp.scheme = appearancePage.schemes[index]
                }
                Text { text: qsTr("Font size:"); font: Theme.fonts.ui }
                LunaComboBox {
                    objectName: "fontSizeCombo"
                    model: [qsTr("Normal"), qsTr("Large Fonts")]
                    currentIndex: dp.fontSize === "large" ? 1 : 0
                    onActivated: (index) => dp.fontSize = index === 1 ? "large" : "normal"
                }
            }
        }

        // --- Settings -----------------------------------------------------
        Item {
            id: settingsPage
            objectName: "page:settings"
            property string title: qsTr("Settings")
            // Distinct sizes of the output's modes, smallest first.
            readonly property var sizes: {
                if (!dp.output)
                    return []
                const seen = {}
                const out = []
                for (const m of dp.output.modes) {
                    const key = m.width + "x" + m.height
                    if (!seen[key]) {
                        seen[key] = true
                        out.push({ width: m.width, height: m.height })
                    }
                }
                if (out.length === 0 && dp.output.width > 0)
                    out.push({ width: dp.output.width, height: dp.output.height })
                out.sort((a, b) => a.width * a.height - b.width * b.height)
                return out
            }
            readonly property int sizeIndex: {
                for (let i = 0; i < sizes.length; ++i) {
                    if (sizes[i].width === dp.outWidth && sizes[i].height === dp.outHeight)
                        return i
                }
                return -1
            }
            readonly property var scales: [1, 1.25, 1.5, 2] // theme-ok: output scale factors

            Text {
                id: dragHint
                width: parent.width
                wrapMode: Text.Wrap
                font: Theme.fonts.ui
                text: qsTr("Drag the monitor icons to match the physical arrangement of your monitors.")
            }
            // Monitor arrangement (one row, numbered like XP).
            Rectangle {
                id: arrangement
                anchors.top: dragHint.bottom
                anchors.topMargin: dp.gap
                width: parent.width
                height: Theme.px(Theme.tokens.controls.monitor.height) / 2
                color: Theme.tokens.desktop.background
                border.color: Theme.tokens.controls.textField.borderColor
                border.width: Theme.px(1)
                Row {
                    anchors.centerIn: parent
                    spacing: dp.pad
                    Repeater {
                        model: Shell.outputs.outputs
                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            width: Theme.px(Theme.tokens.controls.monitor.width) / 2
                            height: width * 3 / 4 // theme-ok: 4:3 monitor icon
                            color: Theme.tokens.colors.selection
                            border.color: Theme.tokens.colors.white
                            border.width: Theme.px(2)
                            Text {
                                anchors.centerIn: parent
                                text: String(parent.index + 1)
                                font: Theme.fonts.welcomeTitle
                                color: Theme.tokens.colors.white
                            }
                        }
                    }
                }
            }
            Text {
                id: displayLabel
                anchors.top: arrangement.bottom
                anchors.topMargin: dp.gap
                width: parent.width
                elide: Text.ElideRight
                font: Theme.fonts.ui
                text: dp.output ? qsTr("Display: 1. %1 on %2").arg(dp.output.description || qsTr("Monitor")).arg(dp.output.name)
                                : qsTr("Display: none found")
            }
            Row {
                id: settingsRow
                anchors.top: displayLabel.bottom
                anchors.topMargin: dp.gap
                width: parent.width
                spacing: dp.pad
                LunaGroupBox {
                    width: (parent.width - dp.pad) / 2
                    title: qsTr("Screen resolution")
                    titleBackground: dp.paneColor
                    Column {
                        spacing: dp.gap
                        LunaSlider {
                            objectName: "resolutionSlider"
                            enabled: settingsPage.sizes.length > 1
                            width: (settingsRow.width - dp.pad) / 2 - Theme.px(Theme.tokens.controls.groupBox.padding) * 2
                            from: 0
                            to: Math.max(1, settingsPage.sizes.length - 1)
                            stepSize: 1
                            snapMode: LunaSlider.SnapAlways
                            tickCount: settingsPage.sizes.length
                            value: Math.max(0, settingsPage.sizeIndex)
                            onMoved: {
                                const s = settingsPage.sizes[Math.round(value)]
                                if (s) {
                                    dp.outWidth = s.width
                                    dp.outHeight = s.height
                                }
                            }
                        }
                        Text {
                            font: Theme.fonts.ui
                            text: qsTr("%1 by %2 pixels").arg(dp.outWidth).arg(dp.outHeight)
                        }
                    }
                }
                LunaGroupBox {
                    width: (parent.width - dp.pad) / 2
                    title: qsTr("Scale")
                    titleBackground: dp.paneColor
                    LunaComboBox {
                        objectName: "scaleCombo"
                        model: settingsPage.scales.map(s => qsTr("%1%").arg(Math.round(s * 100))) // theme-ok: percent
                        currentIndex: Math.max(0, settingsPage.scales.indexOf(dp.outScale))
                        onActivated: (index) => dp.outScale = settingsPage.scales[index]
                    }
                }
            }
            Text {
                anchors.top: settingsRow.bottom
                anchors.topMargin: dp.gap
                width: parent.width
                wrapMode: Text.Wrap
                font: Theme.fonts.ui
                color: Theme.tokens.colors.disabledText
                visible: !Shell.outputs.available
                text: qsTr("This compositor does not let Meadow change display settings.")
            }
        }
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: dp.pad
        anchors.right: buttons.left
        anchors.verticalCenter: buttons.verticalCenter
        visible: dp.outputError.length > 0
        text: dp.outputError
        elide: Text.ElideRight
        font: Theme.fonts.ui
        color: Theme.tokens.dialogs.errorTextColor
    }

    Row {
        id: buttons
        anchors.right: parent.right
        anchors.rightMargin: dp.pad
        anchors.bottom: parent.bottom
        anchors.bottomMargin: dp.pad
        spacing: Theme.px(Theme.tokens.dialogs.buttonSpacing)
        LunaButton { objectName: "dialogButton:OK"; text: qsTr("OK"); isDefault: true; onClicked: dp.accept() }
        LunaButton { objectName: "dialogButton:Cancel"; text: qsTr("Cancel"); onClicked: dp.done() }
        LunaButton { objectName: "dialogButton:Apply"; text: qsTr("Apply"); enabled: dp.dirty; onClicked: dp.apply() }
    }

    // "Keep these settings?" after a resolution change; reverts on its own.
    Rectangle {
        anchors.fill: parent
        visible: dp.keepSeconds > 0
        color: Theme.tokens.dialogs.dimOverlayColor
        MouseArea { anchors.fill: parent }
        LunaDialog {
            objectName: "keepSettings"
            anchors.centerIn: parent
            width: implicitWidth
            height: implicitHeight
            title: qsTr("Monitor Settings")
            buttons: [qsTr("Yes"), qsTr("No")]
            onButtonClicked: (index) => index === 0 ? dp.keepOutput() : dp.revertOutput()
            Text {
                width: Theme.px(Theme.tokens.dialogs.message.width) - Theme.px(Theme.tokens.dialogs.padding) * 2
                wrapMode: Text.Wrap
                font: Theme.fonts.ui
                text: qsTr("Your desktop has been reconfigured. Do you want to keep these settings?\n\nReverting in %n seconds.", "", dp.keepSeconds)
            }
        }
    }
}
