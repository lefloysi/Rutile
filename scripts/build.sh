#!/usr/bin/env bash
# Build the Rutile C/C++ runtime.
#
# Most of the project is orchestrated from shell scripts; CMake is invoked
# only for the C/C++ pieces under bindings/c and runtime/. Future language
# bindings under bindings/{zig,rust,...} are built with their own toolchains
# and should be added to this script as separate phases.

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/out/build"
config="${1:-Debug}"

cmake -S "${repo_root}" -B "${build_dir}" -DCMAKE_BUILD_TYPE="${config}"
cmake --build "${build_dir}" --config "${config}" --parallel
