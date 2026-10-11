# SPDX-License-Identifier: GPL-3.0-or-later
# Still widget windows for the toolkit-theme goldens (tools/toolkit-screenshot.sh);
# not installed. gtk-demo.py needs only python3-gobject at run time.
find_package(Qt6 ${MEADOW_QT_MIN_VERSION} QUIET COMPONENTS Widgets)
if(Qt6Widgets_FOUND)
    qt_add_executable(meadow-qt-demo qt-demo.cpp)
    target_link_libraries(meadow-qt-demo PRIVATE Qt6::Widgets)
else()
    message(STATUS "Qt6 Widgets not found: the Kvantum screenshot tests are skipped")
endif()
