# SPDX-License-Identifier: GPL-3.0-or-later
# Minimal Wayland client with titled, fixed-size windows; tools/session-screenshot.sh
# uses it to show labwc's Meadow decorations (not installed).
qt_add_executable(meadow-decor-test main.cpp)
target_link_libraries(meadow-decor-test PRIVATE meadow-common Qt6::Gui)
