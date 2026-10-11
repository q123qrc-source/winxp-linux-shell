// SPDX-License-Identifier: GPL-3.0-or-later
// The lock screen: the Welcome screen (Meadow.Welcome's WelcomeLayout) in
// lock mode, showing only the locked user. Shown on an ext-session-lock
// surface per screen in a session (Session.qml) and over the preview window
// in --preview (Main.qml). Secondary screens show the background only.
//
// Unlocking: the layout logs in through Shell.lockAuth (PAM service
// meadow-lock, run by meadow-auth-helper). On success the screen shows
// "welcome", fades out and asks Shell.lock to unlock.
import QtQuick
import Meadow.Controls
import Meadow.Welcome
import Meadow.Shell

FocusScope {
    id: root

    property bool primary: true

    readonly property var tk: Theme.tokens.lockScreen

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Meadow is locked")

    LoggingCategory {
        id: lcLock
        name: "meadow.shell.lock"
        defaultLogLevel: LoggingCategory.Info
    }

    UserAccount {
        id: account
        userName: Shell.lockAuth.userName
        onLoadedChanged: root.fillUsers()
    }

    // WelcomeLayout's users model (SDDM roles), with just the locked user.
    ListModel { id: lockUsers }

    function fillUsers() {
        lockUsers.clear()
        lockUsers.append({
            name: Shell.lockAuth.userName,
            realName: account.realName,
            icon: account.iconFile.length > 0 ? "file://" + account.iconFile : "",
            needsPassword: true
        })
    }

    // WelcomeLayout's keyboardModel, backed by the language bar's service
    // (whose current layout is always the first).
    QtObject {
        id: keyboardAdapter
        readonly property bool enabled: Shell.keyboard.count > 0
        readonly property bool capsLock: false
        readonly property var layouts: Shell.keyboard.layouts.map(l => ({ shortName: l.shortName, longName: l.name }))
        property int currentLayout: 0
        onCurrentLayoutChanged: {
            if (currentLayout > 0) {
                Shell.keyboard.select(currentLayout)
                currentLayout = 0
            }
        }
    }

    // A fresh layout for every lock, so nothing from the last one remains.
    Loader {
        id: layoutLoader
        anchors.fill: parent
        focus: true
        active: Shell.lock.active
        opacity: 1
        sourceComponent: WelcomeLayout {
            mode: "lock"
            focus: true
            showUi: root.primary
            greeter: Shell.lockAuth
            power: Shell.power
            users: lockUsers
            keyboardModel: keyboardAdapter
            showSwitchUser: Shell.session.canSwitchUser
            onSwitchUserRequested: Shell.session.switchUser()
            Component.onCompleted: {
                root.fillUsers()
                if (root.primary)
                    Qt.callLater(() => selectUser(0))
            }
        }
        onActiveChanged: if (active) opacity = 1
    }

    NumberAnimation {
        id: fadeOut
        target: layoutLoader
        property: "opacity"
        to: 0
        duration: Theme.reducedMotion ? 0 : root.tk.unlockFadeMs
        onFinished: Shell.lock.unlock()
    }

    Connections {
        target: Shell.lockAuth
        enabled: root.primary
        function onLoginSucceeded() {
            console.info(lcLock, "unlocking")
            fadeOut.start()
        }
    }

    // Entry points for tests and the preview tool.
    function submitPassword(text) {
        if (layoutLoader.item)
            layoutLoader.item.submitPassword(text)
    }
    readonly property Item layout: layoutLoader.item
}
