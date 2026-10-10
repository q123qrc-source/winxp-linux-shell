# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.24)

project(meadow
    VERSION 0.1.0
    DESCRIPTION "An XP-inspired desktop shell, greeter and theme set for Wayland"
    HOMEPAGE_URL "https://github.com/q123qrc-source/winxp-linux-shell"
    LANGUAGES C CXX)  # C: wayland-scanner protocol code

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

# Qt 6.5 is the supported minimum (Fedora ships newer). The cache variable only
# exists so a developer on an older distro can smoke-test the skeleton.
set(MEADOW_QT_MIN_VERSION "6.5" CACHE STRING "Minimum Qt 6 version")

option(MEADOW_BUILD_TESTS   "Build unit tests"                         ON)
option(MEADOW_BUILD_SHELL   "Build meadow-shell"                       ON)
option(MEADOW_BUILD_GREETER "Install the SDDM greeter theme"           ON)
option(MEADOW_BUILD_THEMES  "Install theme tokens and toolkit themes"  ON)
option(MEADOW_DEV_MODE      "Enable live theme reload and dev logging" OFF)

find_package(Qt6 ${MEADOW_QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 DBus)
if(MEADOW_BUILD_TESTS)
    find_package(Qt6 ${MEADOW_QT_MIN_VERSION} REQUIRED COMPONENTS Test)
endif()

# Optional now; each becomes REQUIRED in the phase that first uses it
# (WaylandClient/LayerShellQt: phase 6, Multimedia: phase 2, LinguistTools: phase 7).
find_package(Qt6 ${MEADOW_QT_MIN_VERSION} QUIET OPTIONAL_COMPONENTS WaylandClient Multimedia LinguistTools)
find_package(LayerShellQt QUIET)

option(MEADOW_WITH_WAYLAND    "Use Qt Wayland client APIs"          ${Qt6WaylandClient_FOUND})
option(MEADOW_WITH_LAYERSHELL "Use LayerShellQt for shell surfaces" ${LayerShellQt_FOUND})
option(MEADOW_WITH_MULTIMEDIA "Play theme sounds via QtMultimedia"  ${Qt6Multimedia_FOUND})

if(Qt6_VERSION VERSION_GREATER_EQUAL 6.5)
    qt_standard_project_setup(REQUIRES 6.5)
else()
    qt_standard_project_setup()
endif()
# qt_standard_project_setup() adds $ORIGIN-relative RPATHs. Meadow installs its
# library into the system libdir, so packaged binaries need none (Fedora forbids them).
option(MEADOW_INSTALL_RPATH "Keep Qt's \$ORIGIN install RPATH" OFF)
if(NOT MEADOW_INSTALL_RPATH)
    set(CMAKE_INSTALL_RPATH "")
endif()
# Newer Qt policies we already comply with (silences dev warnings on Qt >= 6.8).
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

# Install locations, overridable by packagers.
set(MEADOW_DATADIR "${CMAKE_INSTALL_DATADIR}/meadow" CACHE PATH "Meadow data directory")
set(MEADOW_QML_INSTALL_DIR "${QT6_INSTALL_QML}" CACHE PATH "QML module install directory")
set(MEADOW_SDDM_THEME_DIR "${CMAKE_INSTALL_DATADIR}/sddm/themes/meadow" CACHE PATH "SDDM theme directory")

add_subdirectory(common)
if(MEADOW_BUILD_SHELL)
    # The shell needs Wayland and layer-shell (taskbar, popups).
    find_package(Qt6 ${MEADOW_QT_MIN_VERSION} REQUIRED COMPONENTS WaylandClient)
    find_package(LayerShellQt 6.0 REQUIRED)
    add_subdirectory(shell)
endif()
option(MEADOW_BUILD_GALLERY "Build the control gallery and screenshot tools" ON)
if(MEADOW_BUILD_GALLERY)
    add_subdirectory(tools/gallery)
    add_subdirectory(tools/greeter-preview)
    add_subdirectory(tools/decor-test)
endif()

# --- Data-only components -------------------------------------------------
if(MEADOW_BUILD_THEMES)
    foreach(scheme luna-blue luna-olive luna-silver)
        install(FILES theme/${scheme}/theme.json DESTINATION ${MEADOW_DATADIR}/themes/${scheme})
    endforeach()
    install(FILES theme/schema.json DESTINATION ${MEADOW_DATADIR}/themes)
    install(FILES theme/assets/meadow-logo.svg DESTINATION ${MEADOW_DATADIR}/assets)
    install(DIRECTORY theme/assets/ DESTINATION ${MEADOW_DATADIR}/assets FILES_MATCHING PATTERN "avatar-*.svg")
    add_subdirectory(toolkit-themes)
    # labwc decorations generated from theme.json (tools/gen-labwc-theme.py).
    foreach(theme Meadow-Luna Meadow-Luna-Olive Meadow-Luna-Silver)
        install(DIRECTORY decorations/labwc/${theme}/labwc
                DESTINATION ${CMAKE_INSTALL_DATADIR}/themes/${theme})
    endforeach()
    install(FILES system/fontconfig/50-meadow.conf
            DESTINATION ${CMAKE_INSTALL_DATADIR}/fontconfig/conf.avail)
endif()

if(MEADOW_BUILD_GREETER)
    install(DIRECTORY greeter/
            DESTINATION ${MEADOW_SDDM_THEME_DIR})
endif()

if(MEADOW_BUILD_SHELL)
    install(FILES session/meadow.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/wayland-sessions)
    install(PROGRAMS session/meadow-session setup/meadow-setup DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES session/labwc/rc.xml session/labwc/menu.xml session/labwc/environment DESTINATION ${MEADOW_DATADIR}/labwc)
    install(PROGRAMS session/labwc/autostart DESTINATION ${MEADOW_DATADIR}/labwc)
    install(FILES session/xdg/meadow-portals.conf DESTINATION ${CMAKE_INSTALL_DATADIR}/xdg-desktop-portal)
    # System integration files are installed as examples only. meadow-setup
    # copies them into /etc on explicit request; packages never touch /etc.
    install(FILES system/sddm.conf.d/10-meadow.conf.example DESTINATION ${MEADOW_DATADIR}/sddm.conf.d)
    install(FILES system/pam.d/sddm.meadow-snippet system/pam.d/meadow-lock DESTINATION ${MEADOW_DATADIR}/pam.d)
    install(FILES system/polkit/50-meadow.rules DESTINATION ${MEADOW_DATADIR}/polkit)
endif()

if(MEADOW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

include(FeatureSummary)
add_feature_info(QtWaylandClient MEADOW_WITH_WAYLAND   "Qt Wayland client (foreign-toplevel, session-lock)")
add_feature_info(LayerShell      MEADOW_WITH_LAYERSHELL "LayerShellQt shell surfaces")
add_feature_info(Multimedia      MEADOW_WITH_MULTIMEDIA "Theme sounds")
feature_summary(WHAT ENABLED_FEATURES DISABLED_FEATURES)
