#!/usr/bin/env bash
set -euo pipefail

configuration=Release
if [[ $# -gt 1 ]]; then
	printf 'Usage: %s [Debug|Release]\n' "$0" >&2
	exit 2
fi
if [[ $# -eq 1 ]]; then
	configuration=$1
fi
case "$configuration" in
	Debug|Release) ;;
	*)
		printf 'Configuration must be Debug or Release.\n' >&2
		exit 2
		;;
esac

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "$script_dir/../.." && pwd)"
version_file="$root/VERSION"
if [[ ! -f "$version_file" ]]; then
	printf 'Missing Rutile version file: %s\n' "$version_file" >&2
	exit 1
fi
expected_version="$(tr -d '\r\n' < "$version_file")"
if [[ ! "$expected_version" =~ ^0\.0\.[1-9][0-9]*$ ]]; then
	printf 'VERSION has an invalid Rutile release version: %s\n' "$expected_version" >&2
	exit 1
fi

program_files_x86="$(printenv 'ProgramFiles(x86)' || true)"
if [[ -z "$program_files_x86" ]]; then
	printf 'ProgramFiles(x86) is not defined; run this script from Git Bash on Windows.\n' >&2
	exit 1
fi
vswhere_win="$program_files_x86/Microsoft Visual Studio/Installer/vswhere.exe"
if ! command -v cygpath >/dev/null 2>&1; then
	printf 'cygpath is required to locate Visual Studio from Bash.\n' >&2
	exit 1
fi
vswhere="$(cygpath -u "$vswhere_win")"
if [[ ! -f "$vswhere" ]]; then
	printf 'Visual Studio locator not found: %s\n' "$vswhere_win" >&2
	exit 1
fi
installation_win="$("$vswhere" -latest -products '*' -requires Microsoft.VisualStudio.Component.VSSDK -property installationPath | tr -d '\r')"
if [[ -z "$installation_win" ]]; then
	printf 'Visual Studio 2022 with the extension development workload is required.\n' >&2
	exit 1
fi
installation="$(cygpath -u "$installation_win")"
msbuild="$installation/MSBuild/Current/Bin/MSBuild.exe"
if [[ ! -f "$msbuild" ]]; then
	printf 'MSBuild was not found: %s\n' "$msbuild" >&2
	exit 1
fi

msbuild_win="$(cygpath -w "$msbuild")"
project_win="$(cygpath -w "$script_dir/Rutile.VisualStudio.csproj")"
MSYS2_ARG_CONV_EXCL='*' "$msbuild_win" "$project_win" /restore /p:NuGetAudit=false /p:RestoreIgnoreFailedSources=true '/t:Build;CreateVsixContainer' "/p:Configuration=$configuration" /p:DeployExtension=false

vsix="$script_dir/bin/$configuration/Rutile.VisualStudio.vsix"
if [[ ! -f "$vsix" ]]; then
	printf 'The Visual Studio SDK did not produce %s\n' "$vsix" >&2
	exit 1
fi
if ! command -v unzip >/dev/null 2>&1; then
	printf 'unzip is required to inspect the packaged VSIX.\n' >&2
	exit 1
fi
manifest="$(unzip -p "$vsix" extension.vsixmanifest)"
if [[ "$manifest" != *"Id=\"Rutile.VisualStudio\" Version=\"$expected_version\""* ]]; then
	printf 'Packaged Rutile extension identity/version does not match VERSION.\n' >&2
	exit 1
fi
entries="$(unzip -Z1 "$vsix")"
if ! grep -Fxq 'Rutile.VisualStudio.dll' <<< "$entries"; then
	printf 'The VSIX is missing its Rutile editor component.\n' >&2
	exit 1
fi
while IFS= read -r entry; do
	case "${entry##*/}" in
		kiln.exe|rutilec.exe|LLVM-C.dll|Kiln.VisualStudio.dll)
			printf 'The Rutile VSIX must not bundle compiler or Kiln payload: %s\n' "$entry" >&2
			exit 1
			;;
	esac
done <<< "$entries"

if [[ "$configuration" == Release ]]; then
	distribution="$root/distro/windows-x64"
	mkdir -p "$distribution"
	cp -- "$vsix" "$distribution/Rutile.VisualStudio.vsix"
fi
printf '%s\n' "$vsix"
