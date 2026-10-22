#!/usr/bin/env bash
# MuslimNotify build script (Linux / macOS).
# SPDX-License-Identifier: MIT
set -euo pipefail

cd "$(dirname "$0")/.."
REPO="$(pwd)"

command -v cargo >/dev/null 2>&1 || { echo "error: cargo not found — install https://rustup.rs"; exit 1; }
command -v cmake >/dev/null 2>&1 || { echo "error: cmake >= 3.21 required"; exit 1; }

# Locate Qt 6: $QT_PATH wins, else qmake6, else Homebrew, else system default.
if [[ -z "${QT_PATH:-}" ]]; then
    if command -v qmake6 >/dev/null 2>&1; then
        QT_PATH="$(qmake6 -query QT_INSTALL_PREFIX)"
    elif [[ "$(uname)" == "Darwin" ]] && command -v brew >/dev/null 2>&1; then
        QT_PATH="$(brew --prefix qt)"
    else
        QT_PATH="/usr"
    fi
fi
echo "==> Qt 6 at: ${QT_PATH}"

echo "==> Building Rust core (release)…"
cargo build --release

echo "==> Configuring Qt frontend…"
cmake -B cpp/build -S cpp \
    -DCMAKE_PREFIX_PATH="${QT_PATH}" \
    -DCMAKE_BUILD_TYPE=Release \
    "$@"

echo "==> Compiling…"
cmake --build cpp/build -j "$(getconf _NPROCESSORS_ONLN 2>/dev/null || nproc 2>/dev/null || sysctl -n hw.ncpu)"

echo
echo "✔ Done. Binary:"
if [[ "$(uname)" == "Darwin" ]]; then
    echo "   ${REPO}/cpp/build/muslimnotify.app"
else
    echo "   ${REPO}/cpp/build/muslimnotify"
fi
echo
echo "Try: cargo run -p mn-cli -- --lat 21.4225 --lng 39.8262 --method 3 --tz 10800 --city Makkah"
