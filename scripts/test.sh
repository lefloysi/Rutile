#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${repo_root}/out/build"
config="${1:-Debug}"

if [ ! -d "${build_dir}" ]; then
    "${repo_root}/scripts/build.sh" "${config}"
fi

ctest --test-dir "${build_dir}" -C "${config}" --output-on-failure
