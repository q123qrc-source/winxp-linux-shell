# SPDX-License-Identifier: GPL-3.0-or-later
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Run tests against the source tree's themes, never an installed copy.
set(test_env "MEADOW_THEME_DIR=${PROJECT_SOURCE_DIR}/theme;QT_QPA_PLATFORM=offscreen;QT_FORCE_STDERR_LOGGING=1")

function(meadow_add_test name)
    qt_add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARGN})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "${test_env}")
endfunction()

meadow_add_test(test_themeloader meadow-common Qt6::Qml)
meadow_add_test(test_controls meadow-common Qt6::Qml)
# Tests import Meadow.Controls from the build tree.
target_compile_definitions(test_themeloader PRIVATE MEADOW_BUILD_QML_DIR="${CMAKE_BINARY_DIR}/qml")
target_compile_definitions(test_controls PRIVATE MEADOW_BUILD_QML_DIR="${CMAKE_BINARY_DIR}/qml")
if(MEADOW_BUILD_SHELL)
    meadow_add_test(test_appmenu meadow-shell-core)
    meadow_add_test(test_settings meadow-shell-core)
    target_compile_definitions(test_settings PRIVATE MEADOW_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    meadow_add_test(test_apps meadow-shell-core)
    target_compile_definitions(test_apps PRIVATE MEADOW_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

    # org.meadow.Shell + meadow-shellctl on a private session bus.
    find_program(DBUS_RUN_SESSION dbus-run-session)
    qt_add_executable(test_shellipc test_shellipc.cpp)
    target_link_libraries(test_shellipc PRIVATE Qt6::Test Qt6::DBus meadow-shell-core)
    target_compile_definitions(test_shellipc PRIVATE MEADOW_SHELLCTL="${PROJECT_SOURCE_DIR}/shell/meadow-shellctl")
    if(DBUS_RUN_SESSION)
        add_test(NAME test_shellipc COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:test_shellipc>)
    else()
        add_test(NAME test_shellipc COMMAND test_shellipc)
    endif()
    set_tests_properties(test_shellipc PROPERTIES ENVIRONMENT "${test_env}")

    # The window list against a real (headless) labwc.
    if(MEADOW_BUILD_GALLERY)
        qt_add_executable(test_toplevel_labwc test_toplevel_labwc.cpp)
        target_link_libraries(test_toplevel_labwc PRIVATE Qt6::Test meadow-shell-core)
        target_compile_definitions(test_toplevel_labwc PRIVATE MEADOW_DECOR_TEST="$<TARGET_FILE:meadow-decor-test>")
        add_test(NAME test_toplevel_labwc
                 COMMAND ${PROJECT_SOURCE_DIR}/tools/with-headless-labwc.sh -- $<TARGET_FILE:test_toplevel_labwc>)
        set_tests_properties(test_toplevel_labwc PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
    endif()

    # Taskbar QML in preview mode, driven by mouse clicks.
    qt_add_executable(test_shell_qml test_shell_qml.cpp)
    target_link_libraries(test_shell_qml PRIVATE Qt6::Test Qt6::Quick meadow-shell-ui meadow-shell-uiplugin)
    target_compile_definitions(test_shell_qml PRIVATE MEADOW_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
                                                      MEADOW_BUILD_QML_DIR="${CMAKE_BINARY_DIR}/qml")
    if(DBUS_RUN_SESSION)
        add_test(NAME test_shell_qml COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:test_shell_qml>)
    else()
        add_test(NAME test_shell_qml COMMAND test_shell_qml)
    endif()
    set_tests_properties(test_shell_qml PROPERTIES ENVIRONMENT "${test_env};QT_QUICK_BACKEND=software" TIMEOUT 120)

    # Tray services and notifications against mocks on a private bus.
    qt_add_executable(test_services test_services.cpp)
    target_link_libraries(test_services PRIVATE Qt6::Test Qt6::DBus meadow-shell-core)
    target_compile_definitions(test_services PRIVATE MEADOW_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
    if(DBUS_RUN_SESSION)
        add_test(NAME test_services COMMAND ${DBUS_RUN_SESSION} -- $<TARGET_FILE:test_services>)
    else()
        add_test(NAME test_services COMMAND test_services)
    endif()
    set_tests_properties(test_services PROPERTIES ENVIRONMENT "${test_env}" TIMEOUT 120)
endif()

add_test(NAME theme_schema
         COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/theme-schema.py check ${PROJECT_SOURCE_DIR}/theme)

add_test(NAME session_desktop COMMAND ${PROJECT_SOURCE_DIR}/tools/validate-session-desktop.sh ${PROJECT_SOURCE_DIR}/session/meadow.desktop)
set_tests_properties(session_desktop PROPERTIES SKIP_RETURN_CODE 77)

meadow_add_test(test_greeter Qt6::Qml Qt6::Quick)
target_compile_definitions(test_greeter PRIVATE MEADOW_BUILD_QML_DIR="${CMAKE_BINARY_DIR}/qml"
                                                MEADOW_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
set_property(TEST test_greeter APPEND PROPERTY ENVIRONMENT "MEADOW_ASSET_DIR=${PROJECT_SOURCE_DIR}/theme/assets")

if(MEADOW_BUILD_GALLERY)
    add_test(NAME greeter_flows
             COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/greeter-flow-test.py ${CMAKE_BINARY_DIR})
    set_tests_properties(greeter_flows PROPERTIES TIMEOUT 300)
endif()
add_test(NAME greeter_sddm_smoke
         COMMAND ${PROJECT_SOURCE_DIR}/tools/greeter-sddm-smoke.sh ${PROJECT_SOURCE_DIR} ${CMAKE_BINARY_DIR})
set_tests_properties(greeter_sddm_smoke PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)

# meadow-setup against sandbox PAM trees (never the real /etc).
add_test(NAME meadow_setup COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/test_meadow_setup.py)

# labwc session: generated theme/rc.xml up to date, config statically valid,
# and accepted by a real (headless) labwc.
add_test(NAME labwc_theme_sync
         COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/gen-labwc-theme.py --check --root ${PROJECT_SOURCE_DIR})
add_test(NAME labwc_config COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/test_labwc_config.py)
add_test(NAME labwc_config_smoke COMMAND ${PROJECT_SOURCE_DIR}/tools/labwc-config-smoke.sh)
set_tests_properties(labwc_config_smoke PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)

add_test(NAME qml_literals
         COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/check-qml-literals.py ${PROJECT_SOURCE_DIR})

if(MEADOW_BUILD_GALLERY)
    # Pixel comparison against tests/golden; font-dependent, so it carries the
    # "screenshot" label and the RPM %check excludes it.
    add_test(NAME screenshot_gallery
             COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/screenshot-test.py --build-dir ${CMAKE_BINARY_DIR})
    set_tests_properties(screenshot_gallery PROPERTIES LABELS screenshot SKIP_RETURN_CODE 77 TIMEOUT 300)
endif()

add_test(NAME asset_policy COMMAND ${PROJECT_SOURCE_DIR}/tools/check-assets.sh ${PROJECT_SOURCE_DIR})

if(MEADOW_BUILD_SHELL)
    add_test(NAME shell_version COMMAND meadow-shell --version)
    # Loads the full QML module offscreen and exits: catches QML import/type errors.
    add_test(NAME shell_preview_smoke COMMAND meadow-shell --preview --exit-after-ms 300)
    set_tests_properties(shell_version shell_preview_smoke PROPERTIES
        ENVIRONMENT "${test_env};QML_IMPORT_PATH=${CMAKE_BINARY_DIR}/qml"
        FAIL_REGULAR_EXPRESSION "(ReferenceError|TypeError|is not a type|module .* is not installed)")
endif()
