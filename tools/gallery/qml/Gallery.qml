// SPDX-License-Identifier: GPL-3.0-or-later
// Every Meadow.Controls control in every state. Sizes here are gallery layout,
// not product metrics, so they may be literals (tools/ is not linted).
import QtQuick
import QtQuick.Window
import QtQuick.Templates as T
import Meadow.Controls

Window {
    id: window
    visible: true
    title: "Meadow control gallery — " + Theme.schemeId + " @ " + Theme.scale + "×"
    width: Theme.px(780)
    height: page.implicitHeight + Theme.px(24)
    color: Theme.tokens.colors.windowBackground

    Column {
        id: page
        x: Theme.px(12)
        y: Theme.px(12)
        width: window.width - Theme.px(24)
        spacing: Theme.px(14)

        // --- Scheme / scale switcher (interactive use) ---
        Row {
            spacing: Theme.px(8)
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "Scheme:"
                font: Theme.fonts.ui
            }
            LunaComboBox {
                model: Theme.availableSchemes()
                currentIndex: Math.max(0, model.indexOf(Theme.schemeId))
                onActivated: (index) => Theme.loadScheme(model[index])
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "Scale:"
                font: Theme.fonts.ui
            }
            LunaComboBox {
                implicitWidth: Theme.px(70)
                model: ["1", "1.25", "1.5", "2"]
                currentIndex: Math.max(0, model.indexOf(String(Theme.scale)))
                onActivated: (index) => Theme.scale = Number(model[index])
            }
        }

        Section {
            title: "LunaButton"
            Labeled { label: "normal"; LunaButton { text: "OK" } }
            Labeled { label: "hover"; LunaButton { text: "OK"; previewState: "hover" } }
            Labeled { label: "pressed"; LunaButton { text: "OK"; previewState: "pressed" } }
            Labeled { label: "focused"; LunaButton { text: "Apply"; previewState: "focus" } }
            Labeled { label: "default"; LunaButton { text: "OK"; isDefault: true } }
            Labeled { label: "disabled"; LunaButton { text: "Cancel"; enabled: false } }
            Labeled { label: "wide text"; LunaButton { text: "Change Settings…" } }
        }

        Section {
            title: "LunaTextField"
            Labeled { label: "placeholder"; LunaTextField { placeholderText: "Type your password" } }
            Labeled { label: "text"; LunaTextField { text: "meadow" } }
            Labeled { label: "focused"; LunaTextField { text: "notepad"; previewState: "focus" } }
            Labeled { label: "disabled"; LunaTextField { text: "read-only"; enabled: false } }
        }

        Section {
            title: "LunaCheckBox"
            Labeled { label: "unchecked"; LunaCheckBox { text: "Lock the taskbar" } }
            Labeled { label: "checked"; LunaCheckBox { text: "Auto-hide"; checked: true } }
            Labeled { label: "hover"; LunaCheckBox { text: "Show Quick Launch"; previewState: "hover" } }
            Labeled { label: "pressed"; LunaCheckBox { text: "Pressed"; checked: true; previewState: "pressed" } }
            Labeled { label: "focused"; LunaCheckBox { text: "Show the clock"; checked: true; previewState: "focus" } }
            Labeled { label: "partial"; LunaCheckBox { text: "Some"; tristate: true; checkState: Qt.PartiallyChecked } }
            Labeled { label: "disabled"; LunaCheckBox { text: "Group buttons"; checked: true; enabled: false } }
        }

        Section {
            title: "LunaComboBox"
            Labeled { label: "normal"; LunaComboBox { model: ["Meadow", "Plasma", "GNOME"] } }
            Labeled { label: "hover"; LunaComboBox { model: ["English (US)"]; previewState: "hover" } }
            Labeled { label: "pressed"; LunaComboBox { model: ["Arabic"]; previewState: "pressed" } }
            Labeled { label: "focused"; LunaComboBox { model: ["Luna Blue"]; previewState: "focus" } }
            Labeled { label: "disabled"; LunaComboBox { model: ["Silver"]; enabled: false } }
        }

        Section {
            title: "LunaScrollBar · LunaTooltip · BalloonTip"
            Labeled {
                label: "normal / hover / pressed"
                Row {
                    spacing: Theme.px(6)
                    Repeater {
                        model: ["", "hover", "pressed"]
                        LunaScrollBar {
                            required property string modelData
                            orientation: Qt.Vertical
                            policy: T.ScrollBar.AlwaysOn
                            size: 0.35
                            position: 0.2
                            height: Theme.px(150)
                            previewState: modelData
                        }
                    }
                }
            }
            Labeled {
                label: "horizontal"
                LunaScrollBar {
                    orientation: Qt.Horizontal
                    policy: T.ScrollBar.AlwaysOn
                    size: 0.4
                    position: 0.5
                    width: Theme.px(150)
                }
            }
            Labeled {
                label: "tooltip"
                Item {
                    width: Theme.px(140)
                    height: Theme.px(40)
                    LunaTooltip {
                        visible: true
                        y: 0
                        text: "Click here to begin"
                    }
                }
            }
            Labeled {
                label: "balloon (tail down, close hover)"
                BalloonTip {
                    visible: true
                    width: Theme.px(240)
                    title: "Did you forget your password?"
                    text: "Your password hint: the meadow behind the house."
                    icon: LunaGlyph.Info
                    previewState: "closeHover"
                    timeout: 0
                }
            }
        }

        Section {
            title: "Glyphs · balloon (tail up) · GlossyGradient"
            Labeled {
                label: "LunaGlyph"
                Grid {
                    columns: 8
                    spacing: Theme.px(4)
                    Repeater {
                        model: [LunaGlyph.Check, LunaGlyph.ArrowUp, LunaGlyph.ArrowDown, LunaGlyph.ArrowLeft,
                                LunaGlyph.ArrowRight, LunaGlyph.ChevronLeft, LunaGlyph.ChevronRight, LunaGlyph.Close,
                                LunaGlyph.Minimize, LunaGlyph.Maximize, LunaGlyph.Restore, LunaGlyph.Info,
                                LunaGlyph.Warning, LunaGlyph.Error, LunaGlyph.Question]
                        GradientBox {
                            required property int modelData
                            width: Theme.px(22)
                            height: Theme.px(22)
                            color: "white"
                            borderWidth: 1
                            borderColor: Theme.tokens.controls.menu.borderColor
                            LunaGlyph {
                                anchors.fill: parent
                                anchors.margins: Theme.px(3)
                                glyph: parent.modelData
                                color: Theme.tokens.colors.windowText
                            }
                        }
                    }
                }
            }
            Labeled {
                label: "balloon (tail up, warning)"
                BalloonTip {
                    visible: true
                    width: Theme.px(220)
                    tailOnTop: true
                    tailX: Theme.px(30)
                    title: "Caps Lock is On"
                    text: "Having Caps Lock on may cause you to enter your password incorrectly."
                    icon: LunaGlyph.Warning
                    timeout: 0
                }
            }
            Labeled {
                label: "taskbar · start (normal/hover/pressed) · task (normal/active/attention) · tray"
                Column {
                    spacing: Theme.px(4)
                    GradientBox {
                        width: Theme.px(420)
                        height: Theme.px(Theme.tokens.taskbar.height)
                        stops: Theme.tokens.taskbar.gradient
                        Row {
                            Repeater {
                                model: ["normal", "hover", "pressed"]
                                GlossyGradient {
                                    required property string modelData
                                    width: Theme.px(Theme.tokens.startButton.width)
                                    height: Theme.px(Theme.tokens.startButton.height)
                                    stops: Theme.tokens.startButton.gradient[modelData]
                                    topLeftRadius: 0
                                    bottomLeftRadius: 0
                                    radius: Theme.px(Theme.tokens.startButton.cornerRadiusRight)
                                    glossColor: modelData === "pressed" ? "transparent" : Theme.tokens.startButton.highlightColor
                                    glossFraction: 0.45
                                    ShadowText {
                                        anchors.centerIn: parent
                                        text: Theme.tokens.startButton.label
                                        font: Theme.fonts.start
                                        color: Theme.tokens.startButton.labelColor
                                        shadowColor: Theme.tokens.startButton.labelShadowColor
                                        shadowOffset: Theme.px(Theme.tokens.startButton.labelShadowOffset)
                                    }
                                }
                            }
                        }
                    }
                    Row {
                        spacing: Theme.px(4)
                        Repeater {
                            model: ["normal", "hover", "active", "attention"]
                            GradientBox {
                                required property string modelData
                                width: Theme.px(110)
                                height: Theme.px(Theme.tokens.taskButton.height)
                                radius: Theme.px(Theme.tokens.taskButton.radius)
                                stops: Theme.tokens.taskButton.gradient[modelData]
                                borderWidth: Theme.px(1)
                                borderColor: modelData === "active" ? Theme.tokens.taskButton.activeBorderColor : Theme.tokens.taskButton.borderColor
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    x: Theme.px(Theme.tokens.taskButton.padding)
                                    text: "Untitled - " + parent.modelData
                                    font: Theme.fonts.ui
                                    color: Theme.tokens.taskButton.textColor
                                }
                            }
                        }
                        GradientBox {
                            width: Theme.px(80)
                            height: Theme.px(Theme.tokens.taskbar.height)
                            stops: Theme.tokens.tray.gradient
                            Text {
                                anchors.centerIn: parent
                                text: "12:00"
                                font: Theme.fonts.ui
                                color: Theme.tokens.clock.textColor
                            }
                        }
                    }
                }
            }
        }

        Section {
            title: "LunaRadioButton · LunaSlider · LunaSpinBox"
            Labeled { label: "radio"; LunaRadioButton { text: "Center" } }
            Labeled { label: "radio checked"; LunaRadioButton { text: "Stretch"; checked: true } }
            Labeled { label: "radio hover"; LunaRadioButton { text: "Tile"; previewState: "hover" } }
            Labeled { label: "radio disabled"; LunaRadioButton { text: "Off"; checked: true; enabled: false } }
            Labeled { label: "slider"; LunaSlider { from: 0; to: 4; stepSize: 1; value: 2; tickCount: 5 } }
            Labeled { label: "slider hover"; LunaSlider { value: 0.7; previewState: "hover" } }
            Labeled { label: "vertical"; LunaSlider { orientation: Qt.Vertical; implicitHeight: Theme.px(80); value: 0.3; tickCount: 3 } }
            Labeled { label: "spin box"; LunaSpinBox { from: 1; to: 60; value: 10 } }
        }

        Section {
            title: "LunaTabView · LunaListView · MonitorPreview"
            Labeled {
                label: "tabs (Appearance hovered)"
                LunaTabView {
                    width: Theme.px(300)
                    height: Theme.px(120)
                    currentIndex: 1
                    Component.onCompleted: tabBar.previewHover = 3
                    Item { property string title: "Themes" }
                    Item {
                        property string title: "Desktop"
                        Text { text: "Page content"; font: Theme.fonts.ui }
                    }
                    Item { property string title: "Screen Saver" }
                    Item { property string title: "Appearance" }
                }
            }
            Labeled {
                label: "list"
                LunaListView {
                    width: Theme.px(160)
                    height: Theme.px(96)
                    currentIndex: 1
                    model: [{ label: "(None)", iconName: "" }, { label: "Meadow Hills", iconName: "image-x-generic" },
                            "Azure", "Coffee Bean", "Red Moon", "Soft Clouds"]
                }
            }
            Labeled {
                label: "monitor"
                MonitorPreview {
                    Rectangle { anchors.fill: parent; color: Theme.tokens.desktop.background }
                }
            }
        }

        Section {
            title: "LunaWindowFrame · LunaDialog"
            Labeled {
                label: "active (close hover)"
                LunaWindowFrame {
                    width: Theme.px(240)
                    height: Theme.px(120)
                    title: "My Documents"
                    previewButtonStates: ({ close: "hover" })
                    Text {
                        x: Theme.px(8)
                        y: Theme.px(8)
                        text: "Client area"
                        font: Theme.fonts.ui
                    }
                }
            }
            Labeled {
                label: "inactive"
                LunaWindowFrame {
                    width: Theme.px(200)
                    height: Theme.px(120)
                    title: "Inactive window"
                    active: false
                    maximized: true
                }
            }
            Labeled {
                label: "dialog"
                LunaDialog {
                    width: implicitWidth
                    height: implicitHeight
                    title: "Run"
                    buttons: ["OK", "Cancel", "Browse…"]
                    Column {
                        spacing: Theme.px(8)
                        Text {
                            width: Theme.px(240)
                            wrapMode: Text.Wrap
                            text: "Type the name of a program, folder, document, or Internet resource, and Meadow will open it for you."
                            font: Theme.fonts.ui
                        }
                        LunaTextField {
                            width: Theme.px(240)
                            text: "pcmanfm-qt"
                        }
                    }
                }
            }
        }
    }
}
