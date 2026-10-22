# MuslimNotify build script (Windows).
# Requires: Rust (rustup), CMake >= 3.21, Qt 6.2+ (Widgets, Network),
# Visual Studio 2019/2022 with C++ tools (or LLVM/MinGW + Ninja).
# SPDX-License-Identifier: MIT
$ErrorActionPreference = "Stop"

Set-Location (Join-Path $PSScriptRoot "..")

if (-not (Get-Command cargo -ErrorAction SilentlyContinue)) {
    throw "cargo not found - install Rust from https://rustup.rs"
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "cmake >= 3.21 required"
}
if (-not $env:QT_PATH) {
    $qmake = Get-Command qmake6 -ErrorAction SilentlyContinue
    if ($qmake) {
        $env:QT_PATH = (& qmake6 -query QT_INSTALL_PREFIX).Trim()
    } else {
        throw "Set `$env:QT_PATH to your Qt install (e.g. C:\Qt\6.7.2\msvc2019_64)"
    }
}
Write-Host "==> Qt 6 at: $env:QT_PATH"

Write-Host "==> Building Rust core (release)..."
cargo build --release

Write-Host "==> Configuring Qt frontend..."
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    cmake -B cpp/build -S cpp `
        -DCMAKE_PREFIX_PATH="$env:QT_PATH" `
        -DCMAKE_BUILD_TYPE=Release `
        -G "Visual Studio 17 2022" -A x64
} else {
    # MinGW/LLVM toolchain with Ninja.
    cmake -B cpp/build -S cpp `
        -DCMAKE_PREFIX_PATH="$env:QT_PATH" `
        -DCMAKE_BUILD_TYPE=Release `
        -G Ninja
}

Write-Host "==> Compiling..."
cmake --build cpp/build --config Release

Write-Host ""
Write-Host "Done. Binary: $(Resolve-Path cpp/build/Release/muslimnotify.exe -ErrorAction SilentlyContinue) or cpp/build/muslimnotify.exe"
Write-Host "Deploy Qt DLLs next to the exe (windeployqt) before distributing."
