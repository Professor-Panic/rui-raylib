cmake_minimum_required(VERSION 3.20)

project(RUI
    VERSION 0.9.0
    DESCRIPTION "Raylib UI Framework"
    LANGUAGES C CXX
)

# ============================================================
# Options
# ============================================================

option(RUI_BUILD_TESTS
    "Build RUI tests"
    OFF
)

option(RUI_BUILD_EXAMPLES
    "Build RUI examples"
    OFF
)

option(RUI_ENABLE_SANITIZERS
    "Enable sanitizers in debug builds"
    OFF
)

option(RUI_INSTALL
    "Generate install targets"
    ON
)

# ============================================================
# Installation directories
# ============================================================

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Default installation location.
# Can still be overridden with:
#
#   -DCMAKE_INSTALL_PREFIX=...
#
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

    set(
        CMAKE_INSTALL_PREFIX
        "C:/Libraries/rui"
        CACHE PATH
        "RUI installation directory"
        FORCE
    )

endif()

# ============================================================
# Raylib configuration
# ============================================================

# ------------------------------------------------------------
# Prebuilt raylib
#
# RUI does NOT build raylib.
#
# Expected:
#
#   C:/raylib/raylib/src/raylib.h
#   C:/raylib/raylib/src/libraylib.a
#
# Override these from CMake if necessary:
#
#   -DRAYLIB_INCLUDE_DIR=...
#   -DRAYLIB_LIBRARY=...
# ------------------------------------------------------------

set(
    RAYLIB_INCLUDE_DIR
    "C:/raylib/raylib/src"
    CACHE PATH
    "Path to prebuilt raylib headers"
)

set(
    RAYLIB_LIBRARY
    "C:/raylib/raylib/src/libraylib.a"
    CACHE FILEPATH
    "Path to prebuilt MinGW raylib library"
)

# ============================================================
# C / C++ standards
# ============================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ============================================================
# Compiler settings
# ============================================================

if(MSVC)

    add_compile_options(
        /W3
        /bigobj
        /utf-8
        /MP
    )

    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
    )

else()

    add_compile_options(
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-unused-function
        -Wno-sign-compare
        -Wno-missing-field-initializers
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")

        add_compile_options(
            -Wa,-mbig-obj
        )

    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")

        add_compile_options(
            -g1
            -fno-var-tracking
            -fno-var-tracking-assignments
        )

    endif()

endif()

# ============================================================
# Validate raylib
# ============================================================

if(NOT EXISTS "${RAYLIB_INCLUDE_DIR}/raylib.h")

    message(FATAL_ERROR
        "RUI: raylib.h was not found.\n"
        "Expected:\n"
        "  ${RAYLIB_INCLUDE_DIR}/raylib.h\n\n"
        "Set RAYLIB_INCLUDE_DIR to the directory containing raylib.h."
    )

endif()

if(NOT EXISTS "${RAYLIB_LIBRARY}")

    message(FATAL_ERROR
        "RUI: prebuilt raylib library was not found.\n"
        "Expected:\n"
        "  ${RAYLIB_LIBRARY}\n\n"
        "Set RAYLIB_LIBRARY to your prebuilt raylib library."
    )

endif()

# ============================================================
# raylib imported target
# ============================================================

if(NOT TARGET raylib)

    add_library(
        raylib
        STATIC
        IMPORTED
        GLOBAL
    )

    set_target_properties(
        raylib
        PROPERTIES

            IMPORTED_LOCATION
                "${RAYLIB_LIBRARY}"

            INTERFACE_INCLUDE_DIRECTORIES
                "${RAYLIB_INCLUDE_DIR}"
    )

endif()

if(NOT TARGET raylib)

    message(FATAL_ERROR
        "RUI: failed to create raylib target."
    )

endif()

# ============================================================
# Optional dependencies
# ============================================================

set(
    IMGUI_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui"
)

set(
    NLOHMANN_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann"
)

# ============================================================
# RUI source files
# ============================================================

set(
    RUI_SOURCES

    src/rui/css_emulator_core.cpp
    src/rui/css_emulator_draw.cpp
    src/rui/css_emulator_input.cpp
    src/rui/css_emulator_layout.cpp
    src/rui/css_emulator_reconcile.cpp
    src/rui/css_emulator_runtime.cpp
    src/rui/css_emulator_runtime_editor.cpp
    src/rui/css_emulator_style.cpp
    src/rui/css_emulator_tree.cpp

    src/rui/css_parser.cpp

    src/rui/rui.cpp
    src/rui/rui_advanced.cpp

    src/rui/ui_animation.cpp
    src/rui/ui_effects.cpp
    src/rui/ui_layout_animation.cpp
    src/rui/ui_markup.cpp
    src/rui/ui_style_values.cpp
    src/rui/ui_viewport.cpp
)

set(
    RUI_HEADERS

    include/component_registry.h
    include/css_emulator.h
    include/css_parser.h
    include/rui.h
    include/ui_markup.h
)

# ============================================================
# Static library
# ============================================================

add_library(
    Rui
    STATIC

    ${RUI_SOURCES}
    ${RUI_HEADERS}
)

add_library(
    Rui::Rui
    ALIAS
    Rui
)

set_target_properties(
    Rui
    PROPERTIES

        OUTPUT_NAME "rui"

        DEBUG_POSTFIX "_d"

        POSITION_INDEPENDENT_CODE ON
)

# ============================================================
# C++ requirements
# ============================================================

target_compile_features(
    Rui
    PUBLIC
        cxx_std_17
)

# ============================================================
# Include directories
# ============================================================

target_include_directories(
    Rui
    PUBLIC

        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>

        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>

    PRIVATE

        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ============================================================
# Optional external includes
# ============================================================

if(EXISTS "${IMGUI_DIR}")

    target_include_directories(
        Rui
        SYSTEM
        PRIVATE
            "${IMGUI_DIR}"
    )

endif()

if(EXISTS "${NLOHMANN_DIR}")

    target_include_directories(
        Rui
        SYSTEM
        PRIVATE
            "${NLOHMANN_DIR}"
    )

endif()

# ============================================================
# Raylib dependency
# ============================================================

# RUI is static and requires raylib.
#
# PUBLIC means consumers of Rui::Rui inherit the raylib
# dependency through the exported target.

target_link_libraries(
    Rui
    PUBLIC
        raylib
)

# ============================================================
# Platform dependencies
# ============================================================

if(WIN32)

    target_link_libraries(
        Rui
        PUBLIC

            gdi32
            winmm
            ole32
            comdlg32
            opengl32
            shell32
    )

elseif(APPLE)

    target_link_libraries(
        Rui
        PUBLIC

            "-framework OpenGL"
            "-framework Cocoa"
            "-framework IOKit"
            "-framework CoreVideo"
    )

elseif(UNIX)

    target_link_libraries(
        Rui
        PUBLIC

            GL
            m
            pthread
            dl
            rt
            X11
    )

endif()

# ============================================================
# Compile definitions
# ============================================================

target_compile_definitions(
    Rui
    PRIVATE

        $<$<CONFIG:Debug>:DEBUG>
        $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

# ============================================================
# Sanitizers
# ============================================================

if(RUI_ENABLE_SANITIZERS)

    if(MSVC)

        message(WARNING
            "RUI_ENABLE_SANITIZERS is not configured for MSVC."
        )

    else()

        target_compile_options(
            Rui
            PRIVATE

                $<$<CONFIG:Debug>:
                    -fsanitize=address,undefined
                >
        )

        target_link_options(
            Rui
            PRIVATE

                $<$<CONFIG:Debug>:
                    -fsanitize=address,undefined
                >
        )

    endif()

endif()

# ============================================================
# Tests
# ============================================================

if(RUI_BUILD_TESTS)

    enable_testing()

    add_subdirectory(
        tests
    )

endif()

# ============================================================
# Examples
# ============================================================

if(RUI_BUILD_EXAMPLES)

    add_subdirectory(
        examples
    )

endif()

# ============================================================
# Installation
# ============================================================

if(RUI_INSTALL)

    # --------------------------------------------------------
    # Install library
    # --------------------------------------------------------

    install(
        TARGETS
            Rui

        EXPORT
            RuiTargets

        ARCHIVE DESTINATION
            ${CMAKE_INSTALL_LIBDIR}

        LIBRARY DESTINATION
            ${CMAKE_INSTALL_LIBDIR}

        RUNTIME DESTINATION
            ${CMAKE_INSTALL_BINDIR}
    )

    # --------------------------------------------------------
    # Install headers
    # --------------------------------------------------------

    install(
        DIRECTORY
            "${CMAKE_CURRENT_SOURCE_DIR}/include/"

        DESTINATION
            "${CMAKE_INSTALL_INCLUDEDIR}"

        FILES_MATCHING
            PATTERN "*.h"
    )

    # --------------------------------------------------------
    # Export target
    # --------------------------------------------------------

    install(
        EXPORT
            RuiTargets

        FILE
            RuiTargets.cmake

        NAMESPACE
            Rui::

        DESTINATION
            "${CMAKE_INSTALL_LIBDIR}/cmake/Rui"
    )

    # --------------------------------------------------------
    # Generate package config
    # --------------------------------------------------------

    set(
        RUI_CONFIG_INPUT
        "${CMAKE_CURRENT_BINARY_DIR}/RuiConfig.in.cmake"
    )

    file(
        WRITE
        "${RUI_CONFIG_INPUT}"

        [=[
@PACKAGE_INIT@

# ============================================================
# RUI dependency: raylib
#
# Rui::Rui requires a CMake target named "raylib".
#
# If the consuming project already created that target, use it.
# Otherwise try to locate a prebuilt raylib installation.
# ============================================================

if(NOT TARGET raylib)

    find_path(
        RAYLIB_INCLUDE_DIR
        NAMES
            raylib.h
    )

    find_library(
        RAYLIB_LIBRARY
        NAMES
            raylib
            libraylib
    )

    if(RAYLIB_INCLUDE_DIR AND RAYLIB_LIBRARY)

        add_library(
            raylib
            STATIC
            IMPORTED
            GLOBAL
        )

        set_target_properties(
            raylib
            PROPERTIES

                IMPORTED_LOCATION
                    "${RAYLIB_LIBRARY}"

                INTERFACE_INCLUDE_DIRECTORIES
                    "${RAYLIB_INCLUDE_DIR}"
        )

    else()

        set(
            Rui_FOUND
            FALSE
        )

        set(
            Rui_NOT_FOUND_MESSAGE

            "Rui requires raylib. "
            "Provide a CMake target named 'raylib', "
            "or make a prebuilt raylib installation discoverable."
        )

        return()

    endif()

endif()

# ============================================================
# RUI targets
# ============================================================

include(
    "${CMAKE_CURRENT_LIST_DIR}/RuiTargets.cmake"
)

check_required_components(Rui)
]=]
    )

    # --------------------------------------------------------
    # Configure package
    # --------------------------------------------------------

    configure_package_config_file(

        "${RUI_CONFIG_INPUT}"

        "${CMAKE_CURRENT_BINARY_DIR}/RuiConfig.cmake"

        INSTALL_DESTINATION
            "${CMAKE_INSTALL_LIBDIR}/cmake/Rui"
    )

    # --------------------------------------------------------
    # Version file
    # --------------------------------------------------------

    write_basic_package_version_file(

        "${CMAKE_CURRENT_BINARY_DIR}/RuiConfigVersion.cmake"

        VERSION
            ${PROJECT_VERSION}

        COMPATIBILITY
            SameMajorVersion
    )

    # --------------------------------------------------------
    # Install config files
    # --------------------------------------------------------

    install(
        FILES

            "${CMAKE_CURRENT_BINARY_DIR}/RuiConfig.cmake"

            "${CMAKE_CURRENT_BINARY_DIR}/RuiConfigVersion.cmake"

        DESTINATION
            "${CMAKE_INSTALL_LIBDIR}/cmake/Rui"
    )

endif()

# ============================================================
# IDE organization
# ============================================================

foreach(header ${RUI_HEADERS})

    source_group(
        "Headers"

        FILES
            ${header}
    )

endforeach()

foreach(source ${RUI_SOURCES})

    source_group(
        "Sources"

        FILES
            ${source}
    )

endforeach()

# ============================================================
# Configuration summary
# ============================================================

message(STATUS "")
message(STATUS "================================================")
message(STATUS "RUI ${PROJECT_VERSION}")
message(STATUS "================================================")
message(STATUS "Compiler:           ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "C++ standard:       C++17")
message(STATUS "C Compiler:         ${CMAKE_C_COMPILER}")
message(STATUS "C++ Compiler:       ${CMAKE_CXX_COMPILER}")
message(STATUS "Raylib include:     ${RAYLIB_INCLUDE_DIR}")
message(STATUS "Raylib library:     ${RAYLIB_LIBRARY}")
message(STATUS "Install prefix:     ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Install lib dir:    ${CMAKE_INSTALL_FULL_LIBDIR}")
message(STATUS "Install include:    ${CMAKE_INSTALL_FULL_INCLUDEDIR}")
message(STATUS "Build tests:        ${RUI_BUILD_TESTS}")
message(STATUS "Build examples:     ${RUI_BUILD_EXAMPLES}")
message(STATUS "Sanitizers:         ${RUI_ENABLE_SANITIZERS}")
message(STATUS "Install:            ${RUI_INSTALL}")
message(STATUS "================================================")
message(STATUS "")