// SPDX-License-Identifier: GPL-3.0-or-later
// A desktop item's Properties sheet (read only): icon and name, type,
// location, size and modified date, from DesktopModel.details().
import QtQuick
import Meadow.Controls

FocusScope {
    id: props

    property var details: ({})

    signal done()

    readonly property var tk: Theme.tokens.dialogs.properties
    readonly property int pad: Theme.px(Theme.tokens.dialogs.padding)

    implicitWidth: Theme.px(tk.width)
    implicitHeight: column.implicitHeight + pad * 2

    Keys.onReturnPressed: done()
    Keys.onEscapePressed: done()

    function sizeText(bytes) {
        if (bytes === undefined)
            return ""
        return qsTr("%1 (%2 bytes)").arg(Qt.locale().formattedDataSize(bytes)).arg(Number(bytes).toLocaleString(Qt.locale(), "f", 0))
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.tokens.dialogs.background
    }
    Column {
        id: column
        x: props.pad
        y: props.pad
        width: parent.width - props.pad * 2
        spacing: Theme.px(props.tk.rowSpacing)
        Row {
            spacing: props.pad
            Image {
                width: Theme.px(props.tk.iconSize)
                height: width
                sourceSize: Qt.size(width, height)
                source: props.details.iconName ? "image://icon/" + props.details.iconName : ""
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                font: Theme.fonts.menuItemBold
                text: props.details.name || ""
            }
        }
        Rectangle { width: parent.width; height: Theme.px(1); color: Theme.tokens.controls.groupBox.borderColor }
        Grid {
            columns: 2
            columnSpacing: props.pad
            rowSpacing: Theme.px(props.tk.rowSpacing)
            visible: !!props.details.location
            Text { width: Theme.px(props.tk.labelWidth); text: qsTr("Type of file:"); font: Theme.fonts.ui }
            Text { text: props.details.type || ""; font: Theme.fonts.ui }
            Text { width: Theme.px(props.tk.labelWidth); text: qsTr("Location:"); font: Theme.fonts.ui }
            Text { text: props.details.location || ""; font: Theme.fonts.ui; elide: Text.ElideMiddle; width: column.width - Theme.px(props.tk.labelWidth) - props.pad }
            Text { width: Theme.px(props.tk.labelWidth); text: qsTr("Size:"); font: Theme.fonts.ui }
            Text { text: props.sizeText(props.details.size); font: Theme.fonts.ui }
            Text { width: Theme.px(props.tk.labelWidth); text: qsTr("Modified:"); font: Theme.fonts.ui }
            Text { text: props.details.modified ? props.details.modified.toLocaleString(Qt.locale(), Locale.LongFormat) : ""; font: Theme.fonts.ui }
        }
        Item { width: 1; height: props.pad }
        LunaButton {
            anchors.right: parent.right
            text: qsTr("OK")
            isDefault: true
            onClicked: props.done()
        }
    }
}
