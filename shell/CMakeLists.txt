# SPDX-License-Identifier: GPL-3.0-or-later

# Non-UI logic lives in a static library so unit tests can link it directly.
qt_add_library(meadow-shell-core STATIC
    src/wayland/ToplevelModel.h src/wayland/ToplevelModel.cpp
    src/wayland/ForeignToplevelModel.h src/wayland/ForeignToplevelModel.cpp
    src/wayland/SessionLock.h src/wayland/SessionLock.cpp
    src/wayland/OutputManager.h src/wayland/OutputManager.cpp
    src/wayland/IdleMonitor.h src/wayland/IdleMonitor.cpp
    src/models/AppMenuModel.h src/models/AppMenuModel.cpp
    src/models/DesktopEntry.h src/models/DesktopEntry.cpp
    src/models/AppRegistry.h src/models/AppRegistry.cpp
    src/models/IconProvider.h src/models/IconProvider.cpp
    src/models/MenuParser.h src/models/MenuParser.cpp
    src/models/UsageStore.h src/models/UsageStore.cpp
    src/models/RecentAppsModel.h src/models/RecentAppsModel.cpp
    src/models/PlacesModel.h src/models/PlacesModel.cpp
    src/models/DesktopModel.h src/models/DesktopModel.cpp
    src/services/Launcher.h src/services/Launcher.cpp
    src/services/RunService.h src/services/RunService.cpp
    src/services/PowerService.h src/services/PowerService.cpp
    src/services/SessionService.h src/services/SessionService.cpp
    src/services/VolumeService.h src/services/VolumeService.cpp
    src/services/NetworkService.h src/services/NetworkService.cpp
    src/services/BatteryService.h src/services/BatteryService.cpp
    src/services/DBusProperties.h src/services/DBusProperties.cpp
    src/services/TrayTypes.h src/services/TrayTypes.cpp
    src/services/StatusNotifierWatcher.h src/services/StatusNotifierWatcher.cpp
    src/services/TrayModel.h src/services/TrayModel.cpp
    src/services/NotificationServer.h src/services/NotificationServer.cpp
    src/services/PolkitAgent.h src/services/PolkitAgent.cpp
    src/services/KeyboardLayoutService.h src/services/KeyboardLayoutService.cpp
    src/services/Settings.h src/services/Settings.cpp
    src/services/LabwcConfig.h src/services/LabwcConfig.cpp
    src/services/TimeDateService.h src/services/TimeDateService.cpp
    src/services/UserAccountsService.h src/services/UserAccountsService.cpp
    src/ipc/ShellIpc.h src/ipc/ShellIpc.cpp
    src/auth/AuthSession.h src/auth/AuthSession.cpp
    src/auth/LockAuthenticator.h src/auth/LockAuthenticator.cpp
)
# D-Bus adaptor for org.meadow.Shell (the object it wraps is ShellIpc).
qt_add_dbus_adaptor(shell_dbus_sources dbus/org.meadow.Shell.xml ipc/ShellIpc.h ShellIpc shelladaptor ShellAdaptor)
target_sources(meadow-shell-core PRIVATE ${shell_dbus_sources})
target_include_directories(meadow-shell-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(meadow-shell-core PUBLIC meadow-common Qt6::Core Qt6::Gui Qt6::DBus Qt6::Quick Qt6::Xml Qt6::WaylandClient
                                        PRIVATE PolkitQt6-1::Agent PolkitQt6-1::Core)
target_compile_definitions(meadow-shell-core PRIVATE MEADOW_DATADIR="${CMAKE_INSTALL_FULL_DATADIR}/meadow"
                                                   MEADOW_LIBEXECDIR="${CMAKE_INSTALL_FULL_LIBEXECDIR}/meadow")

# Window list: wlr-foreign-toplevel-management client code from the protocol
# XML shipped by wlr-protocols (Fedora: wlr-protocols-devel).
find_package(PkgConfig REQUIRED)
pkg_get_variable(WLR_PROTOCOLS_DIR wlr-protocols pkgdatadir)
if(NOT WLR_PROTOCOLS_DIR)
    message(FATAL_ERROR "wlr-protocols not found (dnf install wlr-protocols-devel)")
endif()
pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)
if(NOT WAYLAND_PROTOCOLS_DIR)
    message(FATAL_ERROR "wayland-protocols not found (dnf install wayland-protocols-devel)")
endif()
qt6_generate_wayland_protocol_client_sources(meadow-shell-core
    FILES ${WLR_PROTOCOLS_DIR}/unstable/wlr-foreign-toplevel-management-unstable-v1.xml
          ${WLR_PROTOCOLS_DIR}/unstable/wlr-output-management-unstable-v1.xml
          ${WAYLAND_PROTOCOLS_DIR}/staging/ext-session-lock/ext-session-lock-v1.xml
          ${WAYLAND_PROTOCOLS_DIR}/staging/ext-idle-notify/ext-idle-notify-v1.xml)
# Lock surfaces need a per-window shell integration, which only Qt's private
# Wayland client API offers (as LayerShellQt does); the package pins the Qt
# version accordingly.
target_link_libraries(meadow-shell-core PUBLIC Qt6::WaylandClientPrivate)

# QML-facing glue (Shell singleton, ShellSurface, TrayGlyph) and the
# Meadow.Shell QML module, as a static library so QML tests can load it.
qt_add_library(meadow-shell-ui STATIC
    src/ui/ShellContext.h src/ui/ShellContext.cpp
    src/ui/ShellSurface.h src/ui/ShellSurface.cpp
    src/ui/TrayGlyph.h src/ui/TrayGlyph.cpp
    src/ui/ScreenGrabber.h src/ui/ScreenGrabber.cpp
)
target_link_libraries(meadow-shell-ui PUBLIC meadow-shell-core meadow-common Qt6::Quick LayerShellQt::Interface)
# The generated QML type registrations include the headers by file name.
target_include_directories(meadow-shell-ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/ui)

set(shell_qml
    Main.qml Session.qml
    Taskbar.qml StartButton.qml Grip.qml QuickLaunch.qml TaskList.qml TaskButton.qml TaskGroupMenu.qml
    NotificationArea.qml TrayButton.qml Clock.qml CalendarPopup.qml VolumePopup.qml LanguageBar.qml
    PopupHost.qml ShellMenu.qml TipHost.qml Tips.qml BalloonHost.qml TaskbarProperties.qml
    StartMenu.qml StartMenuUserHeader.qml StartMenuLeftColumn.qml StartMenuRightColumn.qml
    StartMenuItem.qml AllProgramsCascade.qml StartMenuFooter.qml ShellActions.qml
    RunDialog.qml LogOffDialog.qml TurnOffComputerDialog.qml SystemDialogHost.qml
    Desktop.qml DesktopIcon.qml DesktopContextMenu.qml
    AltTabSwitcher.qml LockScreen.qml DisplayProperties.qml ControlPanel.qml
    ScreenSaver.qml ScreenSaverController.qml AppletSheet.qml AnalogClock.qml
    MouseApplet.qml KeyboardApplet.qml SoundsApplet.qml DateTimeApplet.qml UserAccountsApplet.qml
    NetworkApplet.qml AuthenticateDialog.qml ConfirmDialog.qml ItemProperties.qml ShellWindows.qml ShellPreview.qml)
list(TRANSFORM shell_qml PREPEND qml/)
set_source_files_properties(qml/Tips.qml qml/ShellActions.qml qml/ShellPreview.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(meadow-shell-ui
    URI Meadow.Shell
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/Meadow/Shell
    IMPORT_PATH ${CMAKE_BINARY_DIR}/qml
    QML_FILES ${shell_qml}
    DEPENDENCIES QtQuick Meadow.Controls Meadow.Welcome
)

qt_add_executable(meadow-shell src/main.cpp)
target_link_libraries(meadow-shell PRIVATE meadow-shell-ui meadow-shell-uiplugin)
# Meadow.Welcome (lock screen) is loaded at run time from the import path.
add_dependencies(meadow-shell meadow-welcome)

# PAM conversation helper for the lock screen (see auth-helper/). Plain C++,
# no Qt, not setuid.
find_library(PAM_LIBRARY pam REQUIRED)
find_path(PAM_INCLUDE_DIR security/pam_appl.h REQUIRED)
add_executable(meadow-auth-helper auth-helper/meadow-auth-helper.cpp)
target_include_directories(meadow-auth-helper PRIVATE ${PAM_INCLUDE_DIR})
target_link_libraries(meadow-auth-helper PRIVATE ${PAM_LIBRARY})
install(TARGETS meadow-auth-helper RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/meadow)

install(TARGETS meadow-shell RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(PROGRAMS meadow-shellctl meadow-control-panel DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/applications/meadow-control-panel.desktop data/applications/meadow-display-properties.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
# The lock screen's PAM service (password only; meadow-setup adds fingerprint).
install(FILES ${PROJECT_SOURCE_DIR}/system/pam.d/meadow-lock DESTINATION ${CMAKE_INSTALL_FULL_SYSCONFDIR}/pam.d)
install(FILES dbus/org.meadow.Shell.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/interfaces)
install(FILES data/shell.conf DESTINATION ${MEADOW_DATADIR})
