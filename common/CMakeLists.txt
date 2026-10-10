# SPDX-License-Identifier: GPL-3.0-or-later
# libmeadow-common: theme loading and sound playback shared by the greeter and
# the shell, plus the Meadow.Controls QML module (Luna-styled controls).

qt_add_library(meadow-common SHARED
    src/Logging.h src/Logging.cpp
    src/ThemeLoader.h src/ThemeLoader.cpp
    src/SoundPlayer.h src/SoundPlayer.cpp
)

set_target_properties(meadow-common PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

target_compile_definitions(meadow-common
    PRIVATE MEADOW_COMMON_LIBRARY
            MEADOW_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}/meadow"
    PUBLIC  MEADOW_VERSION="${PROJECT_VERSION}")
if(MEADOW_DEV_MODE)
    target_compile_definitions(meadow-common PUBLIC MEADOW_DEV_MODE)
endif()

target_include_directories(meadow-common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
target_link_libraries(meadow-common PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)

set(controls_qml
    LunaButton.qml LunaTextField.qml LunaWindowFrame.qml LunaDialog.qml
    LunaCheckBox.qml LunaComboBox.qml LunaScrollBar.qml LunaTooltip.qml
    BalloonTip.qml GlossyGradient.qml)
list(TRANSFORM controls_qml PREPEND qml/Meadow/Controls/)

# The qmldir file is generated from this call; no hand-written qmldir is kept.
qt_add_qml_module(meadow-common
    URI Meadow.Controls
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/Meadow/Controls
    QML_FILES ${controls_qml}
)

install(TARGETS meadow-common
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    NAMELINK_SKIP)
install(TARGETS meadow-commonplugin
    LIBRARY DESTINATION ${MEADOW_QML_INSTALL_DIR}/Meadow/Controls)
install(FILES
    ${CMAKE_BINARY_DIR}/qml/Meadow/Controls/qmldir
    ${CMAKE_BINARY_DIR}/qml/Meadow/Controls/meadow-common.qmltypes
    DESTINATION ${MEADOW_QML_INSTALL_DIR}/Meadow/Controls)
