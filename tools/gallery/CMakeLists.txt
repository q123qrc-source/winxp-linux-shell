# SPDX-License-Identifier: GPL-3.0-or-later
# Control gallery (developer tool, not installed) and the image diff helper
# used by tools/screenshot-test.py.

qt_add_executable(meadow-gallery main.cpp)
target_link_libraries(meadow-gallery PRIVATE meadow-common Qt6::Quick)

qt_add_qml_module(meadow-gallery
    URI Meadow.Gallery
    VERSION 1.0
    QML_FILES qml/Gallery.qml qml/Section.qml qml/Labeled.qml
)

qt_add_executable(meadow-imgdiff imgdiff.cpp)
target_link_libraries(meadow-imgdiff PRIVATE Qt6::Gui)
