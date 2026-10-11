#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Static checks of Meadow's labwc session config and generated themes.

labwc 0.9 logs invalid actions but silently ignores unknown rc.xml options
and unknown themerc keys, so those are checked here against labwc's
documented names (labwc-actions(5), labwc-theme(5), 0.9.x)."""
import re
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RC = ROOT / "session/labwc/rc.xml"
MENU = ROOT / "session/labwc/menu.xml"
THEMES = ROOT / "decorations/labwc"

LABWC_ACTIONS = set("""
Close Kill Execute Exit Focus Unfocus Raise Lower Iconify Move MoveToEdge Resize ResizeRelative GrowToEdge
ShrinkToEdge MoveTo ResizeTo MoveToCursor MoveRelative ToggleSnapToEdge SnapToEdge ToggleSnapToRegion
SnapToRegion UnSnap NextWindow PreviousWindow Reconfigure ShowMenu SetDecorations ToggleDecorations
ToggleFullscreen ToggleMaximize Maximize UnMaximize ToggleAlwaysOnTop ToggleAlwaysOnBottom ToggleOmnipresent
ToggleKeybinds ToggleTearing FocusOutput MoveToOutput FitToOutput GoToDesktop SendToDesktop AutoPlace Shade
Unshade ToggleShade WarpCursor HideCursor EnableScrollWheelEmulation DisableScrollWheelEmulation
ToggleScrollWheelEmulation EnableTabletMouseEmulation DisableTabletMouseEmulation ToggleTabletMouseEmulation
ToggleMagnify ZoomIn ZoomOut None If ForEach
""".split())

# labwc-theme(5) 0.9: element names plus the gradient fields of textures.
LABWC_THEME_KEYS = set("""
border.color border.width menu.border.color menu.border.width menu.items.active.bg.color
menu.items.active.text.color menu.items.bg.color menu.items.padding.x menu.items.padding.y menu.items.text.color
menu.overlap.x menu.overlap.y menu.separator.color menu.separator.padding.height menu.separator.padding.width
menu.separator.width menu.title.bg.color menu.title.text.color menu.title.text.justify menu.width.max
menu.width.min osd.bg.color osd.border.color osd.border.width osd.label.text.color
osd.window-switcher.style-classic.item.active.bg.color osd.window-switcher.style-classic.item.active.border.color
osd.window-switcher.style-classic.item.active.border.width osd.window-switcher.style-classic.item.icon.size
osd.window-switcher.style-classic.item.padding.x osd.window-switcher.style-classic.item.padding.y
osd.window-switcher.style-classic.padding osd.window-switcher.style-classic.width
window.active.border.color window.active.button.unpressed.image.color
window.active.indicator.toggled-keybind.color window.active.label.text.color window.active.shadow.color
window.active.shadow.size window.active.title.bg window.active.title.bg.color window.button.height
window.button.hover.bg.color window.button.hover.bg.corner-radius window.button.spacing window.button.width
window.inactive.border.color window.inactive.button.unpressed.image.color window.inactive.label.text.color
window.inactive.shadow.color window.inactive.shadow.size window.inactive.title.bg window.inactive.title.bg.color
window.label.text.justify window.titlebar.padding.height window.titlebar.padding.width
""".split())
for state in ("active", "inactive"):
    for field in ("color.splitTo", "colorTo", "colorTo.splitTo"):
        LABWC_THEME_KEYS.add(f"window.{state}.title.bg.{field}")

BUTTON_FILES = {f"{kind}{hover}-{state}.svg" for kind in ("iconify", "max", "max_toggled", "close")
                for hover in ("", "_hover") for state in ("active", "inactive")}

# Keybinds the brief requires: key -> (action, command or None).
REQUIRED = {
    "Super_L": ("Execute", "meadow-shellctl toggle-start-menu"),
    "W-d": ("Execute", "meadow-shellctl show-desktop"),
    "W-e": ("Execute", "pcmanfm-qt"),
    "W-r": ("Execute", "meadow-shellctl run"),
    "W-l": ("Execute", "meadow-shellctl lock"),
    "A-Tab": ("Execute", "meadow-shellctl switch-window next"),
    "A-S-Tab": ("Execute", "meadow-shellctl switch-window prev"),
    "W-Tab": ("NextWindow", None),
    "A-F4": ("Close", None),
    "C-A-Delete": ("Execute", "meadow-shellctl log-off"),
    "A-Shift_L": ("Execute", "meadow-shellctl next-layout"),
    "A-space": ("ShowMenu", None),
}


def shellctl_commands():
    text = (ROOT / "shell/meadow-shellctl").read_text()
    return set(re.findall(r"^\s+([a-z-]+)\)\s+method=", text, re.M))


class RcXmlTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = ET.parse(RC).getroot()
        cls.binds = {kb.get("key"): kb for kb in cls.root.iter("keybind")}

    def test_required_keybinds(self):
        for key, (action, command) in REQUIRED.items():
            with self.subTest(key=key):
                self.assertIn(key, self.binds, f"missing keybind {key}")
                act = self.binds[key].find("action")
                self.assertEqual(act.get("name"), action)
                if command:
                    self.assertEqual(act.get("command"), command)
        self.assertEqual(self.binds["Super_L"].get("onRelease"), "yes")

    def test_actions_exist_in_labwc(self):
        for act in self.root.iter("action"):
            self.assertIn(act.get("name"), LABWC_ACTIONS, ET.tostring(act).decode())

    def test_execute_commands_are_known(self):
        known_programs = {"meadow-shellctl", "pcmanfm-qt", "wpctl"}
        sub = shellctl_commands()
        self.assertIn("toggle-start-menu", sub)
        for act in self.root.iter("action"):
            if act.get("name") != "Execute":
                continue
            argv = act.get("command").split()
            self.assertIn(argv[0], known_programs, argv)
            if argv[0] == "meadow-shellctl":
                self.assertIn(argv[1], sub, f"meadow-shellctl has no '{argv[1]}' command")

    def test_no_duplicate_keybinds(self):
        keys = [kb.get("key") for kb in self.root.iter("keybind")]
        self.assertEqual(len(keys), len(set(keys)))

    def test_focus_and_placement(self):
        self.assertEqual(self.root.findtext("focus/followMouse"), "no")
        self.assertEqual(self.root.findtext("placement/policy"), "automatic")
        self.assertEqual(self.root.findtext("theme/titlebar/layout"), "icon:iconify,max,close")

    def test_theme_exists(self):
        name = self.root.findtext("theme/name")
        self.assertTrue((THEMES / name / "labwc/themerc").exists(), name)

    def test_title_doubleclick_maximizes(self):
        ctx = [c for c in self.root.iter("context") if c.get("name") == "Title"][0]
        binds = {(m.get("button"), m.get("action")): m.find("action").get("name") for m in ctx.iter("mousebind")}
        self.assertEqual(binds[("Left", "DoubleClick")], "ToggleMaximize")


class MenuTests(unittest.TestCase):
    def test_client_menu(self):
        root = ET.parse(MENU).getroot()
        menu = [m for m in root.iter("menu") if m.get("id") == "client-menu"][0]
        names = [a.get("name") for a in menu.iter("action")]
        for needed in ("Iconify", "ToggleMaximize", "Move", "Resize", "Close"):
            self.assertIn(needed, names)
        for name in names:
            self.assertIn(name, LABWC_ACTIONS)


class ThemeTests(unittest.TestCase):
    def test_each_theme_complete_and_known_keys(self):
        themes = sorted(p for p in THEMES.iterdir() if (p / "labwc/themerc").exists())
        self.assertEqual([t.name for t in themes], ["Meadow-Luna", "Meadow-Luna-Olive", "Meadow-Luna-Silver"])
        for theme in themes:
            with self.subTest(theme=theme.name):
                files = {p.name for p in (theme / "labwc").iterdir()}
                self.assertTrue(BUTTON_FILES <= files, BUTTON_FILES - files)
                for line in (theme / "labwc/themerc").read_text().splitlines():
                    if not line.strip() or line.startswith("#"):
                        continue
                    key, value = (part.strip() for part in line.split(":", 1))
                    self.assertIn(key, LABWC_THEME_KEYS, f"{theme.name}: labwc would ignore {key}")
                    if key.endswith("color") or ".color" in key or key.endswith("colorTo"):
                        self.assertRegex(value, r"^#[0-9a-f]{6}([0-9a-f]{2})?$", key)

    def test_title_height_is_30(self):
        text = (THEMES / "Meadow-Luna/labwc/themerc").read_text()
        values = dict(line.split(": ", 1) for line in text.splitlines() if ": " in line and not line.startswith("#"))
        self.assertEqual(int(values["window.button.height"]) + 2 * int(values["window.titlebar.padding.height"]), 30)


if __name__ == "__main__":
    unittest.main(verbosity=2)
