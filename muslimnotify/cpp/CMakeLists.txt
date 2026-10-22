# MuslimNotify — Qt Widgets frontend over the Rust core.
# SPDX-License-Identifier: MIT
cmake_minimum_required(VERSION 3.21)
project(MuslimNotify VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Network)
qt_standard_project_setup()

include(RustStaticLib)
mn_setup_rust()

add_executable(muslimnotify
    src/main.cpp
    src/appstate.h    src/appstate.cpp
    src/mainwindow.h  src/mainwindow.cpp
    src/settingsdialog.h src/settingsdialog.cpp
    src/audioplayer.h src/audioplayer.cpp
    src/icons.h       src/icons.cpp
    assets.qrc
)

target_include_directories(muslimnotify PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../include")

target_compile_definitions(muslimnotify PRIVATE
    MN_VERSION_PROJECT="${PROJECT_VERSION}")

target_link_libraries(muslimnotify PRIVATE
    Qt6::Widgets
    Qt6::Network
    ${MN_RUST_LIBRARY}
)
add_dependencies(muslimnotify rust_ffi)

# Rust std support libraries per platform.
if(UNIX AND NOT APPLE)
    target_link_libraries(muslimnotify PRIVATE pthread dl m)
elseif(APPLE)
    target_link_libraries(muslimnotify PRIVATE "-framework CoreFoundation" "-framework Security"
        "-framework SystemConfiguration" resolv)
elseif(WIN32)
    target_link_libraries(muslimnotify PRIVATE ws2_32 bcrypt ntdll userenv
        synchronization advapi32 ole32 shell32)
endif()

# Deployment hints: macOS bundle metadata.
set_target_properties(muslimnotify PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "MuslimNotify"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "org.muslimnotify.MuslimNotify"
    MACOSX_BUNDLE_ICON_FILE "" )
