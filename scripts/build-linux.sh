#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
vcpkg_root="${VCPKG_ROOT:-"$HOME/.cache/rutile/vcpkg"}"
preset="${1:-linux-ci}"

if ! command -v cmake >/dev/null; then
    echo "cmake is required. Install it with: sudo apt install cmake" >&2
    exit 1
fi

if ! command -v ninja >/dev/null; then
    echo "ninja is required. Install it with: sudo apt install ninja-build" >&2
    exit 1
fi

if ! command -v git >/dev/null; then
    echo "git is required. Install it with: sudo apt install git" >&2
    exit 1
fi

if [ ! -x "$vcpkg_root/vcpkg" ]; then
    mkdir -p "$(dirname "$vcpkg_root")"
    if [ ! -d "$vcpkg_root/.git" ]; then
        git clone https://github.com/microsoft/vcpkg "$vcpkg_root"
    fi
    "$vcpkg_root/bootstrap-vcpkg.sh" -disableMetrics
fi

export VCPKG_ROOT="$vcpkg_root"

cmake --fresh --preset "$preset"
cmake --build --preset "$preset"
ctest --preset "$preset"
