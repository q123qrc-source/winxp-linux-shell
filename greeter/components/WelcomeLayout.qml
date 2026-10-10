// SPDX-License-Identifier: GPL-3.0-or-later
// The Welcome screen: dark top and bottom bands with thin divider lines, a
// lighter middle with a soft glow, the logo and "click your user name" on the
// left, the user list on the right, and power/session/keyboard controls in
// the bottom band.
//
// Keyboard: Up/Down move between users, Enter selects (or logs on), Esc goes
// back, Alt+Shift switches keyboard layout, Tab reaches the bottom controls.
import QtQuick
import Meadow.Controls

FocusScope {
    id: layout

    // SDDM objects (see Main.qml).
    property var greeter
    property var users
    property var sessions
    property var keyboardModel

    // "welcome" (gradient layout), "solid", or an image path/URL.
    property string background: "welcome"
    // false on secondary screens: background only.
    property bool showUi: true
    property bool showSessionSwitcher: true
    property bool showKeyboardLayout: true
    property bool highContrast: false
    property bool showAllPowerActions: false

    property int selectedIndex: -1
    property bool busy: false
    property bool loggedIn: false

    readonly property var tk: Theme.tokens.welcome
    readonly property var colors: Theme.tokens.colors
    readonly property int topHeight: Theme.px(tk.topBand.height)
    readonly property int bottomHeight: Theme.px(tk.bottomBand.height)
    readonly property int gap: Theme.px(tk.layout.panelGap)
    readonly property var selectedTile: selectedIndex >= 0 ? userList.itemAtIndex(selectedIndex) : null
    readonly property int sessionIndex: sessionSwitcher.visible ? sessionSwitcher.currentIndex
                                                                : (sessions ? sessions.lastIndex : 0)
    readonly property bool isImageBackground: background !== "welcome" && background !== "solid" && background.length > 0

    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    LoggingCategory {
        id: lcAuth
        name: "meadow.greeter.auth"
        defaultLogLevel: LoggingCategory.Info
    }

    UserAccount {
        id: account
        userName: layout.selectedTile ? layout.selectedTile.userName : ""
    }

    SoundPlayer { id: sounds }

    // --- Actions ----------------------------------------------------------

    function selectUser(index) {
        if (busy || !users || index < 0 || index >= userList.count)
            return
        balloon.close()
        userList.currentIndex = index
        userList.positionViewAtIndex(index, ListView.Contain)
        selectedIndex = index
        const tile = userList.itemAtIndex(index)
        if (!tile)
            return
        if (!tile.needsPassword) {
            login("")
            return
        }
        Qt.callLater(() => { if (layout.selectedTile) layout.selectedTile.passwordPanel.focusField() })
    }

    function deselect() {
        if (busy)
            return
        if (selectedTile)
            selectedTile.passwordPanel.clear()
        selectedIndex = -1
        balloon.close()
        userList.forceActiveFocus()
    }

    function submitPassword(text) {
        if (selectedIndex >= 0)
            login(text)
    }

    function login(password) {
        const tile = selectedTile
        if (!tile || busy)
            return
        busy = true
        balloon.close()
        // Never log the password or anything derived from it.
        console.info(lcAuth, "login requested for", tile.userName, "session", sessionIndex)
        greeter.login(tile.userName, password, sessionIndex)
    }

    function openTurnOffDialog() {
        if (showUi)
            turnOffDialog.open()
    }

    // Point just below the selected password field, in layout coordinates.
    function balloonAnchor() {
        if (!selectedTile || !selectedTile.passwordPanel.visible)
            return Qt.point(width / 2, height / 2)
        const field = selectedTile.passwordPanel.field
        return field.mapToItem(layout, field.width / 2, field.height)
    }

    Connections {
        target: layout.greeter
        ignoreUnknownSignals: true
        function onLoginFailed() {
            console.info(lcAuth, "login failed")
            layout.busy = false
            const tile = layout.selectedTile
            if (!tile)
                return
            tile.passwordPanel.clear()
            tile.passwordPanel.shake()
            balloon.showLoginFailed(account.passwordHint)
            tile.passwordPanel.focusField()
        }
        function onLoginSucceeded() {
            console.info(lcAuth, "login succeeded")
            layout.loggedIn = true
            sounds.play("logon")
        }
        function onInformationMessage(message) {
            balloon.showMessage(message)
        }
    }

    Keys.onPressed: (event) => {
        const altShift = (event.key === Qt.Key_Shift && (event.modifiers & Qt.AltModifier))
                      || (event.key === Qt.Key_Alt && (event.modifiers & Qt.ShiftModifier))
        if (altShift) {
            keyboardSwitcher.cycle()
            event.accepted = true
        }
    }

    // --- Background -------------------------------------------------------

    GradientBox {
        anchors.fill: parent
        color: layout.highContrast ? layout.colors.highContrastBackground : layout.tk.background
    }

    Image {
        anchors.fill: parent
        visible: layout.isImageBackground && !layout.highContrast
        source: layout.isImageBackground ? (layout.background.indexOf(":/") > 0 ? layout.background : "file://" + layout.background) : ""
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
    }

    Item {
        id: middle
        anchors.left: parent.left
        anchors.right: parent.right
        y: layout.topHeight
        height: parent.height - layout.topHeight - layout.bottomHeight

        GradientBox {
            anchors.fill: parent
            visible: layout.background === "welcome" && !layout.highContrast
            radial: true
            centerX: layout.tk.glow.centerX
            centerY: layout.tk.glow.centerY
            radiusRatio: layout.tk.glow.radius
            stops: [{ pos: 0, color: layout.tk.glow.color }, { pos: 1, color: layout.tk.glow.edgeColor }]
        }
    }

    GradientBox {
        id: topBand
        anchors.left: parent.left
        anchors.right: parent.right
        height: layout.topHeight
        color: layout.highContrast ? layout.colors.highContrastBackground : layout.tk.topBand.color
    }
    GradientBox {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: topBand.bottom
        height: Theme.px(layout.tk.dividerTop.height)
        orientation: Qt.Horizontal
        stops: layout.highContrast ? [] : layout.tk.dividerTop.gradient
        color: layout.colors.highContrastAccent
    }

    GradientBox {
        id: bottomBand
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: layout.bottomHeight
        stops: layout.highContrast ? [] : layout.tk.bottomBand.gradient
        color: layout.colors.highContrastBackground
    }
    GradientBox {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: bottomBand.top
        height: Theme.px(layout.tk.dividerBottom.height)
        orientation: Qt.Horizontal
        stops: layout.highContrast ? [] : layout.tk.dividerBottom.gradient
        color: layout.colors.highContrastAccent
    }

    // --- Middle: logo, hint, divider, users -------------------------------

    Item {
        anchors.fill: middle
        visible: layout.showUi && !layout.loggedIn

        Column {
            id: leftPanel
            anchors.right: parent.horizontalCenter
            anchors.rightMargin: layout.gap
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(layout.tk.layout.hintSpacing)

            Logo {
                anchors.right: parent.right
                monochrome: layout.highContrast ? layout.colors.highContrastText : "transparent"
            }
            Text {
                anchors.right: parent.right
                width: Math.min(implicitWidth, Theme.px(layout.tk.layout.hintMaxWidth))
                text: layout.selectedIndex >= 0 ? qsTr("Type your password, then press Enter")
                                                : qsTr("To begin, click your user name")
                font: Theme.fonts.welcomeHint
                color: layout.highContrast ? layout.colors.highContrastText : layout.tk.hintColor
                horizontalAlignment: Text.AlignRight
                wrapMode: Text.Wrap
            }
        }

        GradientBox {
            id: divider
            x: parent.width / 2
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: layout.gap
            anchors.bottomMargin: layout.gap
            width: Theme.px(layout.tk.verticalDivider.width)
            stops: layout.highContrast ? [] : [
                { pos: 0, color: layout.tk.verticalDivider.edgeColor },
                { pos: layout.tk.verticalDivider.fadeLength, color: layout.tk.verticalDivider.color },
                { pos: 1 - layout.tk.verticalDivider.fadeLength, color: layout.tk.verticalDivider.color },
                { pos: 1, color: layout.tk.verticalDivider.edgeColor }
            ]
            color: layout.colors.highContrastAccent
        }

        ListView {
            id: userList
            anchors.left: parent.horizontalCenter
            anchors.leftMargin: layout.gap
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.px(layout.tk.tile.width) + Theme.px(Theme.tokens.controls.scrollBar.width)
            height: Math.min(contentHeight, parent.height - layout.gap * 2)
            spacing: Theme.px(layout.tk.tile.spacing)
            clip: true
            focus: true
            keyNavigationEnabled: !layout.busy
            boundsBehavior: Flickable.StopAtBounds
            model: layout.users
            currentIndex: layout.users && layout.users.lastIndex >= 0 ? layout.users.lastIndex : 0

            Accessible.role: Accessible.List
            Accessible.name: qsTr("User accounts")

            Keys.onReturnPressed: layout.selectUser(currentIndex)
            Keys.onEnterPressed: layout.selectUser(currentIndex)
            Keys.onEscapePressed: layout.deselect()
            onCurrentIndexChanged: {
                // Moving to another user while one is open opens that user.
                if (layout.selectedIndex >= 0 && currentIndex !== layout.selectedIndex)
                    layout.selectUser(currentIndex)
            }

            LunaScrollBar.vertical: LunaScrollBar { policy: LunaScrollBar.AsNeeded }

            delegate: UserTile {
                // SDDM roles: name, realName, homeDir, icon, needsPassword.
                // Read through `model` so no role shadows a UserTile property.
                required property int index
                required property var model

                userName: model.name
                displayName: model.realName && model.realName.length > 0 ? model.realName : model.name
                iconSource: model.icon ? model.icon : ""
                needsPassword: model.needsPassword
                selected: layout.selectedIndex === index
                current: ListView.isCurrentItem && userList.activeFocus && layout.selectedIndex < 0
                dimmed: layout.selectedIndex >= 0 && layout.selectedIndex !== index
                busy: layout.busy && selected
                highContrast: layout.highContrast
                onActivated: layout.selectUser(index)
                onSubmitted: (password) => layout.login(password)
                onHintRequested: balloon.showHint(account.passwordHint)
                onCancelled: layout.deselect()
            }
        }
    }

    // Shown after a successful login, until SDDM closes the greeter.
    ShadowText {
        anchors.centerIn: middle
        visible: layout.loggedIn
        text: qsTr("welcome")
        font: Theme.fonts.welcomeTitle
        color: layout.tk.successText.color
        shadowColor: layout.tk.successText.shadowColor
        shadowOffset: Theme.px(2)
    }

    // --- Bottom band ------------------------------------------------------

    Item {
        anchors.fill: bottomBand
        visible: layout.showUi

        TurnOffButton {
            id: turnOffButton
            anchors.left: parent.left
            anchors.leftMargin: Theme.px(layout.tk.layout.sidePadding)
            anchors.verticalCenter: parent.verticalCenter
            highContrast: layout.highContrast
            onClicked: layout.openTurnOffDialog()
            KeyNavigation.tab: sessionSwitcher.visible ? sessionSwitcher : userList
        }

        Row {
            anchors.left: turnOffButton.right
            anchors.leftMargin: Theme.px(layout.tk.layout.sidePadding)
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.px(layout.tk.layout.bottomControlsSpacing)

            SessionSwitcher {
                id: sessionSwitcher
                visible: layout.showSessionSwitcher && layout.sessions !== undefined && layout.sessions !== null && layout.sessions.count > 0
                model: layout.sessions
                currentIndex: layout.sessions && layout.sessions.lastIndex >= 0 ? layout.sessions.lastIndex : 0
                highContrast: layout.highContrast
            }
            KeyboardLayoutSwitcher {
                id: keyboardSwitcher
                visible: layout.showKeyboardLayout && implicitWidth > 0 && keyboardModel && keyboardModel.enabled && layouts.length > 0
                keyboardModel: layout.keyboardModel
                highContrast: layout.highContrast
            }
            FingerprintIndicator {
                id: fingerprint
            }
            PillButton {
                id: oskButton
                visible: oskLoader.status === Loader.Ready
                glyph: LunaGlyph.Keyboard
                highContrast: layout.highContrast
                checkable: true
                Accessible.name: qsTr("On-screen keyboard")
            }
            PillButton {
                glyph: LunaGlyph.Contrast
                highContrast: layout.highContrast
                onClicked: layout.highContrast = !layout.highContrast
                Accessible.name: layout.highContrast ? qsTr("Turn off high contrast") : qsTr("Turn on high contrast")
            }
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: Theme.px(layout.tk.layout.sidePadding)
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, Theme.px(layout.tk.layout.infoMaxWidth))
            text: qsTr("After you log on, you can change your picture and password in Control Panel under User Accounts.")
            font: Theme.fonts.ui
            color: layout.highContrast ? layout.colors.highContrastText : layout.tk.infoColor
            horizontalAlignment: Text.AlignRight
            wrapMode: Text.Wrap
        }
    }

    // --- Overlays ---------------------------------------------------------

    CapsLockWarning {
        readonly property point anchorPoint: layout.selectedTile ? layout.balloonAnchor() : Qt.point(0, 0)
        visible: layout.showUi && !layout.busy && !balloon.visible && layout.selectedTile !== null
                 && layout.selectedTile.passwordPanel.visible
                 && layout.keyboardModel !== undefined && layout.keyboardModel !== null && layout.keyboardModel.capsLock
        x: anchorPoint.x - tailX
        y: anchorPoint.y
        tailX: Theme.px(Theme.tokens.controls.balloon.tailWidth) * 2
        opacity: 1
    }

    MessageBalloon {
        id: balloon
        readonly property point anchorPoint: visible ? layout.balloonAnchor() : Qt.point(0, 0)
        x: anchorPoint.x - tailX
        y: anchorPoint.y
        tailX: Theme.px(Theme.tokens.controls.balloon.tailWidth) * 2
    }

    TurnOffDialog {
        id: turnOffDialog
        anchors.fill: parent
        greeter: layout.greeter
        showAll: layout.showAllPowerActions
        highContrast: layout.highContrast
        onClosed: turnOffButton.forceActiveFocus()
    }

    Loader {
        id: oskLoader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        active: layout.showUi && Theme.moduleAvailable("QtQuick.VirtualKeyboard")
        source: "VirtualKeyboardPanel.qml"
        onLoaded: item.shown = Qt.binding(() => oskButton.checked)
    }
}
