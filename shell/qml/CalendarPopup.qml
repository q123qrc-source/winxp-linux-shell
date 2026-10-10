// SPDX-License-Identifier: GPL-3.0-or-later
// Calendar shown by clicking the clock: today's long date, a month grid with
// today highlighted, and arrows (or Page Up/Down) to change month.
import QtQuick
import Meadow.Controls
import Meadow.Shell

FocusScope {
    id: popup

    property var host: null
    objectName: "calendar"
    // First day of the month shown.
    property date month: new Date(Shell.now.getFullYear(), Shell.now.getMonth(), 1)

    readonly property int week: 7 // theme-ok: days in a week
    readonly property var tk: Theme.tokens.clock.calendar
    readonly property int pad: Theme.px(tk.padding)
    readonly property int cellW: Theme.px(tk.cellWidth)
    readonly property int cellH: Theme.px(tk.cellHeight)
    readonly property int firstWeekday: Qt.locale().firstDayOfWeek
    // Grid offset of day 1 (0..6) in a locale-ordered week.
    readonly property int lead: (month.getDay() - firstWeekday + week) % week
    readonly property int daysInMonth: new Date(month.getFullYear(), month.getMonth() + 1, 0).getDate()

    width: cellW * week + pad * 2
    height: column.implicitHeight + pad * 2
    focus: true

    Accessible.role: Accessible.Dialog
    Accessible.name: qsTr("Calendar")

    function shift(months) { month = new Date(month.getFullYear(), month.getMonth() + months, 1) }

    Keys.onLeftPressed: shift(-1)
    Keys.onRightPressed: shift(1)
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_PageUp) shift(-1)
        else if (event.key === Qt.Key_PageDown) shift(1)
    }

    Rectangle {
        anchors.fill: parent
        color: popup.tk.background
        border.width: Theme.px(1)
        border.color: popup.tk.borderColor
    }

    Column {
        id: column
        x: popup.pad
        y: popup.pad
        spacing: popup.pad

        Text {
            text: Qt.locale().toString(Shell.now, Theme.tokens.clock.tooltipFormat)
            font.family: Theme.fonts.ui.family
            font.pixelSize: Theme.fonts.ui.pixelSize
            font.bold: true
            color: Theme.tokens.colors.windowText
        }

        // Month header with arrows.
        GradientBox {
            width: popup.cellW * popup.week
            height: popup.cellH + Theme.px(2) * 2
            stops: popup.tk.headerGradient

            Repeater {
                model: [-1, 1]
                delegate: Item {
                    required property int modelData
                    width: popup.cellH
                    height: parent.height
                    x: modelData < 0 ? 0 : parent.width - width
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData < 0 ? qsTr("Previous month") : qsTr("Next month")
                    LunaGlyph {
                        anchors.centerIn: parent
                        width: Theme.px(popup.tk.arrowSize)
                        height: width
                        glyph: parent.modelData < 0 ? LunaGlyph.ArrowLeft : LunaGlyph.ArrowRight
                        color: popup.tk.headerText
                    }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: popup.shift(parent.modelData)
                    }
                }
            }
            Text {
                anchors.centerIn: parent
                text: Qt.locale().standaloneMonthName(popup.month.getMonth()) + " " + popup.month.getFullYear()
                font.family: Theme.fonts.ui.family
                font.pixelSize: Theme.fonts.ui.pixelSize
                font.bold: true
                color: popup.tk.headerText
            }
        }

        Grid {
            columns: popup.week

            // Weekday initials in locale order.
            Repeater {
                model: popup.week
                delegate: Text {
                    required property int index
                    width: popup.cellW
                    height: popup.cellH
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: Qt.locale().standaloneDayName((popup.firstWeekday + index) % popup.week, Locale.NarrowFormat)
                    font: Theme.fonts.ui
                    color: popup.tk.weekdayText
                }
            }
            // Six weeks of days; leading/trailing cells stay empty.
            Repeater {
                model: 42 // theme-ok: six weeks of seven days
                delegate: Item {
                    required property int index
                    readonly property int day: index - popup.lead + 1
                    readonly property bool inMonth: day >= 1 && day <= popup.daysInMonth
                    readonly property bool today: inMonth && day === Shell.now.getDate()
                        && popup.month.getMonth() === Shell.now.getMonth()
                        && popup.month.getFullYear() === Shell.now.getFullYear()
                    width: popup.cellW
                    height: popup.cellH
                    visible: index < 35 || popup.lead + popup.daysInMonth > 35 // theme-ok: five weeks

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: Theme.px(1)
                        visible: parent.today
                        color: popup.tk.todayBackground
                    }
                    Text {
                        anchors.centerIn: parent
                        visible: parent.inMonth
                        text: parent.day
                        font: Theme.fonts.ui
                        color: parent.today ? popup.tk.todayText : Theme.tokens.colors.windowText
                    }
                }
            }
        }
    }
}
