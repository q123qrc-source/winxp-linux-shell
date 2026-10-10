// SPDX-License-Identifier: GPL-3.0-or-later
// On-screen keyboard. Loaded only when QtQuick.VirtualKeyboard is installed;
// SDDM must also use it as input method (InputMethod=qtvirtualkeyboard in
// sddm.conf, see docs/ARCHITECTURE.md).
import QtQuick
import QtQuick.VirtualKeyboard

InputPanel {
    property bool shown: false
    visible: shown || active
}
