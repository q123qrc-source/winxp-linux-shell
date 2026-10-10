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
endif()

add_test(NAME theme_schema
         COMMAND Python3::Interpreter ${PROJECT_SOURCE_DIR}/tools/theme-schema.py check ${PROJECT_SOURCE_DIR}/theme)

add_test(NAME session_desktop COMMAND ${PROJECT_SOURCE_DIR}/tools/validate-session-desktop.sh ${PROJECT_SOURCE_DIR}/session/meadow.desktop)
set_tests_properties(session_desktop PROPERTIES SKIP_RETURN_CODE 77)

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
