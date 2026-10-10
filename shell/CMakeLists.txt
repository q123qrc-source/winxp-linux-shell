# SPDX-License-Identifier: GPL-3.0-or-later

# Non-UI logic lives in a static library so unit tests can link it directly.
qt_add_library(meadow-shell-core STATIC
    src/wayland/ForeignToplevelModel.h src/wayland/ForeignToplevelModel.cpp
    src/wayland/SessionLock.h src/wayland/SessionLock.cpp
    src/models/AppMenuModel.h src/models/AppMenuModel.cpp
    src/models/RecentAppsModel.h src/models/RecentAppsModel.cpp
    src/models/PlacesModel.h src/models/PlacesModel.cpp
    src/services/PowerService.h src/services/PowerService.cpp
    src/services/SessionService.h src/services/SessionService.cpp
    src/services/VolumeService.h src/services/VolumeService.cpp
    src/services/NetworkService.h src/services/NetworkService.cpp
    src/services/BatteryService.h src/services/BatteryService.cpp
    src/services/TrayHost.h src/services/TrayHost.cpp
    src/services/NotificationServer.h src/services/NotificationServer.cpp
    src/services/PolkitAgent.h src/services/PolkitAgent.cpp
    src/services/KeyboardLayoutService.h src/services/KeyboardLayoutService.cpp
    src/services/Settings.h src/services/Settings.cpp
    src/ipc/ShellIpc.h src/ipc/ShellIpc.cpp
)
target_include_directories(meadow-shell-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(meadow-shell-core PUBLIC meadow-common Qt6::Core Qt6::Gui Qt6::DBus)

qt_add_executable(meadow-shell src/main.cpp)
target_link_libraries(meadow-shell PRIVATE meadow-shell-core meadow-common Qt6::Quick)

set(shell_qml
    Main.qml
    Taskbar.qml StartButton.qml QuickLaunch.qml TaskButton.qml TaskGroupMenu.qml
    NotificationArea.qml Clock.qml LanguageBar.qml
    StartMenu.qml StartMenuUserHeader.qml StartMenuLeftColumn.qml StartMenuRightColumn.qml
    AllProgramsCascade.qml StartMenuFooter.qml
    Desktop.qml DesktopIcon.qml DesktopContextMenu.qml
    RunDialog.qml LogOffDialog.qml TurnOffComputerDialog.qml
    AltTabSwitcher.qml LockScreen.qml VolumePopup.qml DisplayProperties.qml
    ControlPanel.qml TaskbarProperties.qml)
list(TRANSFORM shell_qml PREPEND qml/)

qt_add_qml_module(meadow-shell
    URI Meadow.Shell
    VERSION 1.0
    QML_FILES ${shell_qml}
)

install(TARGETS meadow-shell RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
