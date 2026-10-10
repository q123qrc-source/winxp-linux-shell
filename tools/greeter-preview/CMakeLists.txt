# SPDX-License-Identifier: GPL-3.0-or-later
# greeter-preview: runs greeter/Main.qml against mock SDDM objects for fast
# iteration, screenshot tests and scripted interaction tests (not installed).
qt_add_executable(meadow-greeter-preview main.cpp MockSddm.h MockSddm.cpp)
target_link_libraries(meadow-greeter-preview PRIVATE meadow-common Qt6::Quick Qt6::Qml)
target_compile_definitions(meadow-greeter-preview PRIVATE MEADOW_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
