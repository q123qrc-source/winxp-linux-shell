# SPDX-License-Identifier: GPL-3.0-or-later
# libmeadow-common: theme loading and sound playback shared by the greeter and
# the shell, plus the Meadow.Controls QML module (Luna-styled controls).

qt_add_library(meadow-common SHARED
    src/Logging.h src/Logging.cpp
    src/ThemeLoader.h src/ThemeLoader.cpp
    src/ThemeObject.h src/ThemeObject.cpp
    src/SoundPlayer.h src/SoundPlayer.cpp
    src/Perf.h src/Perf.cpp
    src/GradientBox.h src/GradientBox.cpp
    src/LunaGlyph.h src/LunaGlyph.cpp
    src/BalloonShape.h src/BalloonShape.cpp
    src/MeadowLogo.h src/MeadowLogo.cpp
    src/UserAccount.h src/UserAccount.cpp
    src/FingerprintQuery.h src/FingerprintQuery.cpp
)

set_target_properties(meadow-common PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

target_compile_definitions(meadow-common
    PRIVATE MEADOW_COMMON_LIBRARY
            MEADOW_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}/meadow"
            MEADOW_SOUND_DIR="${CMAKE_INSTALL_FULL_DATADIR}/sounds/meadow/stereo"
    PUBLIC  MEADOW_VERSION="${PROJECT_VERSION}")
if(MEADOW_DEV_MODE)
    target_compile_definitions(meadow-common PUBLIC MEADOW_DEV_MODE)
endif()

target_include_directories(meadow-common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
target_link_libraries(meadow-common PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick PRIVATE Qt6::DBus)
if(MEADOW_WITH_MULTIMEDIA)
    target_link_libraries(meadow-common PRIVATE Qt6::Multimedia)
    target_compile_definitions(meadow-common PRIVATE MEADOW_WITH_MULTIMEDIA)
endif()

set(controls_qml
    LunaButton.qml LunaTextField.qml LunaWindowFrame.qml LunaDialog.qml
    LunaCheckBox.qml LunaComboBox.qml LunaScrollBar.qml LunaTooltip.qml
    BalloonTip.qml GlossyGradient.qml ShadowText.qml Logo.qml Avatar.qml LunaGroupBox.qml
    PowerButton.qml PowerDialogFrame.qml TurnOffDialog.qml
    LunaTabBar.qml LunaTabView.qml LunaRadioButton.qml LunaSlider.qml LunaSpinBox.qml
    LunaListView.qml MonitorPreview.qml)
list(TRANSFORM controls_qml PREPEND qml/Meadow/Controls/)

# The qmldir file is generated from this call; no hand-written qmldir is kept.
qt_add_qml_module(meadow-common
    URI Meadow.Controls
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/Meadow/Controls
    QML_FILES ${controls_qml}
    DEPENDENCIES QtQuick QtQuick.Templates
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

# Meadow.Welcome: the Welcome-screen layout and its parts, shared by the SDDM
# greeter and the shell's lock screen. QML only; the plugin is the backing
# target, so the module is one library that both load from the QML import
# path (the system QML dir when installed, build/qml in development).
set(welcome_qml
    WelcomeLayout.qml UserTile.qml PasswordPanel.qml FingerprintIndicator.qml
    CapsLockWarning.qml MessageBalloon.qml PillButton.qml TurnOffButton.qml
    SessionSwitcher.qml KeyboardLayoutSwitcher.qml)
list(TRANSFORM welcome_qml PREPEND qml/Meadow/Welcome/)

qt_add_library(meadow-welcome MODULE)
qt_add_qml_module(meadow-welcome
    URI Meadow.Welcome
    VERSION 1.0
    PLUGIN_TARGET meadow-welcome
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/Meadow/Welcome
    QML_FILES ${welcome_qml}
    DEPENDENCIES QtQuick QtQuick.Templates Meadow.Controls
)
target_link_libraries(meadow-welcome PRIVATE Qt6::Quick)

install(TARGETS meadow-welcome
    LIBRARY DESTINATION ${MEADOW_QML_INSTALL_DIR}/Meadow/Welcome)
install(FILES
    ${CMAKE_BINARY_DIR}/qml/Meadow/Welcome/qmldir
    ${CMAKE_BINARY_DIR}/qml/Meadow/Welcome/meadow-welcome.qmltypes
    DESTINATION ${MEADOW_QML_INSTALL_DIR}/Meadow/Welcome)
