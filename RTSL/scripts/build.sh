#!/usr/bin/env bash
# Build the RTSL compiler library and rtslc CLI.

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/out/build"
config="${1:-Debug}"

cmake -S "${repo_root}" -B "${build_dir}" -DCMAKE_BUILD_TYPE="${config}"
cmake --build "${build_dir}" --config "${config}" --parallel
