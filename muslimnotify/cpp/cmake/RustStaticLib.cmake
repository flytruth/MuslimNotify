# RustStaticLib.cmake — build the mn-ffi Rust crate and expose it for linking.
#
# Sets in parent scope after mn_setup_rust():
#   MN_RUST_LIBRARY  — absolute path to the static archive
#   rust_ffi         — custom target that produces it
#
# SPDX-License-Identifier: MIT

function(mn_setup_rust)
    find_program(CARGO_EXE cargo)
    if(NOT CARGO_EXE)
        message(FATAL_ERROR "cargo not found — install Rust from https://rustup.rs")
    endif()
    execute_process(COMMAND ${CARGO_EXE} --version OUTPUT_VARIABLE CARGO_VER)
    string(STRIP "${CARGO_VER}" CARGO_VER)
    message(STATUS "MuslimNotify Rust core via ${CARGO_VER}")

    # cpp/ is the CMake source dir; the Cargo workspace is its parent.
    set(MN_SOURCE_DIR "${CMAKE_SOURCE_DIR}/..")
    set(MN_TARGET_DIR "${MN_SOURCE_DIR}/target")

    if(WIN32)
        set(RUSTLIB "${MN_TARGET_DIR}/release/muslimnotify_ffi.lib")
    else()
        set(RUSTLIB "${MN_TARGET_DIR}/release/libmuslimnotify_ffi.a")
    endif()

    file(GLOB_RECURSE MN_RUST_SOURCES
        "${MN_SOURCE_DIR}/crates/*.rs"
        "${MN_SOURCE_DIR}/Cargo.toml"
        "${MN_SOURCE_DIR}/crates/*/Cargo.toml")

    add_custom_command(
        OUTPUT "${RUSTLIB}"
        COMMAND ${CMAKE_COMMAND} -E env
            CARGO_TARGET_DIR="${MN_TARGET_DIR}"
            ${CARGO_EXE} build --release --package mn-ffi
        WORKING_DIRECTORY "${MN_SOURCE_DIR}"
        DEPENDS ${MN_RUST_SOURCES}
        USES_TERMINAL
        COMMENT "Building Rust core (mn-ffi, release)…")

    add_custom_target(rust_ffi ALL DEPENDS "${RUSTLIB}")

    set(MN_RUST_LIBRARY "${RUSTLIB}" PARENT_SCOPE)
endfunction()
