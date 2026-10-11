// SPDX-License-Identifier: GPL-3.0-or-later
// "Date and Time Properties": Date & Time (a month calendar and a clock;
// editable while network time is off), Time Zone, and Internet Time
// (automatic synchronization). Changes go to systemd-timedated
// (Shell.timeDate), which asks for administrator authorization through the
// session's polkit agent.
import QtQuick
import Meadow.Controls
import Meadow.Shell

AppletSheet {
    id: applet

    property string zone
    property bool ntp
    // The edited date/time; null while the user has not changed it.
    property var editedTime: null
    readonly property date shownTime: editedTime ? editedTime : Shell.now
    readonly property var zones: Shell.timeDate.timezones

    function load(tab) {
        zone = Shell.timeDate.timezone
        ntp = Shell.timeDate.ntp
        editedTime = null
        message = ""
        showTab(tab)
    }
    dirty: zone !== Shell.timeDate.timezone || ntp !== Shell.timeDate.ntp || editedTime !== null
    onApplyRequested: {
        message = ""
        if (zone !== Shell.timeDate.timezone)
            Shell.timeDate.setTimezone(zone)
        if (ntp !== Shell.timeDate.ntp)
            Shell.timeDate.setNtp(ntp)
        if (editedTime !== null && !ntp)
            Shell.timeDate.setTime(editedTime)
        editedTime = null
    }
    Connections {
        target: Shell.timeDate
        function onFailed(text) { applet.message = text }
    }

    function edit(year, month, day, h, m, s) {
        editedTime = new Date(year, month, day, h, m, s)
    }

    Item {
        id: datePage
        property string title: qsTr("Date & Time")
        property string key: "datetime"
        readonly property bool editable: !applet.ntp
        readonly property int week: 7 // theme-ok: days in a week
        readonly property var ctk: Theme.tokens.clock.calendar
        readonly property date first: new Date(applet.shownTime.getFullYear(), applet.shownTime.getMonth(), 1)
        readonly property int lead: (first.getDay() - Qt.locale().firstDayOfWeek + week) % week
        readonly property int daysInMonth: new Date(first.getFullYear(), first.getMonth() + 1, 0).getDate()

        Row {
            spacing: applet.pad
            LunaGroupBox {
                title: qsTr("Date")
                titleBackground: Theme.tokens.controls.tab.paneBackground
                Column {
                    spacing: applet.pad
                    enabled: datePage.editable
                    Row {
                        spacing: applet.pad
                        LunaComboBox {
                            objectName: "monthCombo"
                            model: Array.from({ length: 12 }, (_, i) => Qt.locale().standaloneMonthName(i)) // theme-ok: months
                            currentIndex: applet.shownTime.getMonth()
                            onActivated: (index) => {
                                const t = applet.shownTime
                                const day = Math.min(t.getDate(), new Date(t.getFullYear(), index + 1, 0).getDate())
                                applet.edit(t.getFullYear(), index, day, t.getHours(), t.getMinutes(), t.getSeconds())
                            }
                        }
                        LunaSpinBox {
                            objectName: "yearSpin"
                            from: Theme.tokens.dialogs.dateTime.firstYear
                            to: Theme.tokens.dialogs.dateTime.lastYear
                            value: applet.shownTime.getFullYear()
                            textFromValue: (v) => String(v)
                            onValueModified: {
                                const t = applet.shownTime
                                applet.edit(value, t.getMonth(), t.getDate(), t.getHours(), t.getMinutes(), t.getSeconds())
                            }
                        }
                    }
                    Grid {
                        columns: datePage.week
                        Repeater {
                            model: datePage.week
                            delegate: Text {
                                required property int index
                                width: Theme.px(datePage.ctk.cellWidth)
                                height: Theme.px(datePage.ctk.cellHeight)
                                horizontalAlignment: Text.AlignHCenter
                                text: Qt.locale().standaloneDayName((Qt.locale().firstDayOfWeek + index) % datePage.week, Locale.NarrowFormat)
                                font: Theme.fonts.ui
                                color: datePage.ctk.weekdayText
                            }
                        }
                        Repeater {
                            model: datePage.week * 6 // theme-ok: six weeks
                            delegate: Rectangle {
                                required property int index
                                readonly property int day: index - datePage.lead + 1
                                readonly property bool inMonth: day >= 1 && day <= datePage.daysInMonth
                                readonly property bool chosen: inMonth && day === applet.shownTime.getDate()
                                width: Theme.px(datePage.ctk.cellWidth)
                                height: Theme.px(datePage.ctk.cellHeight)
                                color: chosen ? datePage.ctk.todayBackground : "transparent"
                                Text {
                                    anchors.centerIn: parent
                                    visible: parent.inMonth
                                    text: parent.day
                                    font: Theme.fonts.ui
                                    color: parent.chosen ? datePage.ctk.todayText : Theme.tokens.colors.windowText
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    enabled: parent.inMonth && datePage.editable
                                    onClicked: {
                                        const t = applet.shownTime
                                        applet.edit(t.getFullYear(), t.getMonth(), parent.day, t.getHours(), t.getMinutes(), t.getSeconds())
                                    }
                                }
                            }
                        }
                    }
                }
            }
            LunaGroupBox {
                title: qsTr("Time")
                titleBackground: Theme.tokens.controls.tab.paneBackground
                Column {
                    spacing: applet.pad
                    AnalogClock {
                        objectName: "analogClock"
                        time: applet.shownTime
                    }
                    LunaTextField {
                        objectName: "timeField"
                        enabled: datePage.editable
                        width: Theme.px(Theme.tokens.dialogs.applet.timeFieldWidth)
                        inputMask: "99:99:99"
                        text: Qt.formatTime(applet.shownTime, "HH:mm:ss")
                        onEditingFinished: {
                            const p = text.split(":").map(Number)
                            const t = applet.shownTime
                            if (p.length === 3 && p[0] < 24 && p[1] < 60 && p[2] < 60) // theme-ok: clock ranges
                                applet.edit(t.getFullYear(), t.getMonth(), t.getDate(), p[0], p[1], p[2])
                        }
                    }
                }
            }
        }
        Text {
            anchors.bottom: parent.bottom
            width: parent.width
            wrapMode: Text.Wrap
            font: Theme.fonts.ui
            text: (applet.ntp ? qsTr("The clock is set automatically from the Internet (see Internet Time).") + "\n" : "")
                  + qsTr("Current time zone: %1").arg(applet.zone)
        }
    }

    Item {
        property string title: qsTr("Time Zone")
        property string key: "timezone"
        Column {
            width: parent.width
            spacing: applet.pad
            Text { text: qsTr("Time zone:"); font: Theme.fonts.ui }
            LunaComboBox {
                objectName: "zoneCombo"
                width: parent.width
                model: applet.zones
                currentIndex: applet.zones.indexOf(applet.zone)
                onActivated: (index) => applet.zone = applet.zones[index]
            }
            Rectangle {
                width: parent.width
                height: Theme.px(Theme.tokens.controls.monitor.height)
                color: Theme.tokens.desktop.background
                border.color: Theme.tokens.controls.textField.borderColor
                border.width: Theme.px(1)
                Text {
                    anchors.centerIn: parent
                    font: Theme.fonts.welcomeHint
                    color: Theme.tokens.colors.white
                    text: applet.zone.replace(/_/g, " ").split("/").pop()
                }
            }
        }
    }

    Item {
        property string title: qsTr("Internet Time")
        property string key: "internet"
        Column {
            width: parent.width
            spacing: applet.pad
            LunaCheckBox {
                objectName: "option:ntp"
                enabled: Shell.timeDate.canNtp
                text: qsTr("Automatically synchronize with an Internet time server")
                checked: applet.ntp
                onToggled: applet.ntp = checked
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                font: Theme.fonts.ui
                color: Shell.timeDate.canNtp ? Theme.tokens.colors.windowText : Theme.tokens.colors.disabledText
                text: Shell.timeDate.canNtp ? qsTr("The system keeps the clock right using network time (systemd-timesyncd or chrony).")
                                            : qsTr("No network time service is installed.")
            }
        }
    }
}
