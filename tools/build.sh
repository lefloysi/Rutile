#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd -- "$script_dir/.." && pwd)"
cd -- "$root"
profile="${2:-debug}"
case "$profile" in
	debug|release) ;;
	*) printf 'Profile must be debug or release.\n' >&2; exit 1 ;;
esac
build_dir="$root/build/$profile"
dependency_dir="$build_dir/dependencies"
toolchain_dir="$build_dir/toolchain"
native_dir="$build_dir/native"
version_file="$root/VERSION"

usage() {
	cat <<'EOF'
Usage:
  bash tools/build.sh prepare [debug|release]
  bash tools/build.sh build [debug|release]
  bash tools/build.sh help

prepare copies installed dependencies and writes bootstrap files beneath build/<profile>/.
build compiles the Rutile sources and builds build/<profile>/native/rutilec.exe.
Set RUTILE_COMPILER to select a seed. Otherwise Rutilec is searched on PATH, then in distro/.
Set RUTILE_DEPENDENCIES to select an installed dependency root.
EOF
}

fail() {
	printf '%s\n' "$1" >&2
	exit 1
}

to_unix_path() {
	local path=$1
	if [[ "$path" == [A-Za-z]:* ]] && command -v cygpath >/dev/null 2>&1; then
		path="$(cygpath -u "$path")"
	fi
	printf '%s\n' "$path"
}

find_vs_installation() {
	if [[ -n "${VSINSTALLDIR:-}" ]]; then
		printf '%s\n' "$(to_unix_path "$VSINSTALLDIR")"
		return
	fi
	command -v cygpath >/dev/null 2>&1 || fail 'cygpath is required. Run this from Git Bash on Windows.'
	local program_files_x86
	program_files_x86="$(printenv 'ProgramFiles(x86)' || true)"
	[[ -n "$program_files_x86" ]] || fail 'ProgramFiles(x86) is not defined.'
	local vswhere
	vswhere="$(cygpath -u "$program_files_x86/Microsoft Visual Studio/Installer/vswhere.exe")"
	[[ -f "$vswhere" ]] || fail "Visual Studio locator not found: $vswhere"
	local installation
	installation="$("$vswhere" -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | tr -d '\r')"
	[[ -n "$installation" ]] || fail 'Visual Studio 2022 C++ build tools are required.'
	to_unix_path "$installation"
}

copy_directory_contents() {
	local source=$1
	local destination=$2
	[[ -d "$source" ]] || return 0
	mkdir -p "$destination"
	if [[ "$(cd -- "$source" && pwd)" != "$(cd -- "$destination" && pwd)" ]]; then
		cp -R -- "$source/." "$destination/"
	fi
}

copy_file() {
	local source=$1
	local destination=$2
	[[ -f "$source" ]] || fail "Required dependency file not found: $source"
	mkdir -p "$(dirname -- "$destination")"
	if [[ "$(cd -- "$(dirname -- "$source")" && pwd)/$(basename -- "$source")" != "$destination" ]]; then
		cp -f -- "$source" "$destination"
	fi
}

prepare_dependencies() {
	local vcpkg_root="$(to_unix_path "${VCPKG_ROOT:-}")"
	local -a roots=( "$dependency_dir" "$root/build/debug/vcpkg_installed/x64-windows" "$root/build/release/vcpkg_installed/x64-windows" )
	if [[ -n "${RUTILE_DEPENDENCIES:-}" ]]; then
		roots=( "$(to_unix_path "$RUTILE_DEPENDENCIES")" "${roots[@]}" )
	fi
	if [[ -n "$vcpkg_root" ]]; then
		roots+=( "$vcpkg_root/installed/x64-windows" "$vcpkg_root" )
	fi
	if [[ -d /d/Tools/vcpkg ]]; then
		roots+=( /d/Tools/vcpkg )
	fi

	local cli_include='' json_include='' cli_library=''
	local root_candidate
	for root_candidate in "${roots[@]}"; do
		if [[ -z "$cli_include" ]]; then
			if [[ -f "$root_candidate/include/CLI/CLI.hpp" ]]; then
				cli_include="$root_candidate/include/CLI"
			elif [[ -f "$root_candidate/packages/cli11_x64-windows/include/CLI/CLI.hpp" ]]; then
				cli_include="$root_candidate/packages/cli11_x64-windows/include/CLI"
			fi
		fi
		if [[ -z "$json_include" ]]; then
			if [[ -f "$root_candidate/include/nlohmann/json.hpp" ]]; then
				json_include="$root_candidate/include/nlohmann"
			elif [[ -f "$root_candidate/packages/nlohmann-json_x64-windows/include/nlohmann/json.hpp" ]]; then
				json_include="$root_candidate/packages/nlohmann-json_x64-windows/include/nlohmann"
			fi
		fi
		if [[ -z "$cli_library" ]]; then
			for candidate in \
				"$root_candidate/lib/CLI11.lib" \
				"$root_candidate/packages/cli11_x64-windows/lib/CLI11.lib" \
				"$root_candidate/packages/cli11_x64-windows-static/lib/CLI11.lib"; do
				if [[ -f "$candidate" ]]; then
					cli_library="$candidate"
					break
				fi
			done
		fi
	done

	local llvm_root="$(to_unix_path "${LLVM_ROOT:-C:/Program Files/LLVM}")"
	local llvm_library=''
	local llvm_dll=''
	for root_candidate in "${roots[@]}" "$llvm_root"; do
		if [[ -z "$llvm_library" ]]; then
			for candidate in "$root_candidate/lib/LLVM-C.lib" "$root_candidate/LLVM-C.lib"; do
				if [[ -f "$candidate" ]]; then
					llvm_library="$candidate"
					break
				fi
			done
		fi
		if [[ -z "$llvm_dll" ]]; then
			for candidate in "$root_candidate/bin/LLVM-C.dll" "$root_candidate/LLVM-C.dll"; do
				if [[ -f "$candidate" ]]; then
					llvm_dll="$candidate"
					break
				fi
			done
		fi
	done
	if [[ -z "$llvm_dll" && -f "$root/distro/windows-x64/LLVM-C.dll" ]]; then
		llvm_dll="$root/distro/windows-x64/LLVM-C.dll"
	fi

	[[ -n "$cli_include" ]] || fail 'CLI11 headers were not found. Set RUTILE_DEPENDENCIES to an existing dependency root.'
	[[ -n "$json_include" ]] || fail 'nlohmann/json headers were not found. Set RUTILE_DEPENDENCIES to an existing dependency root.'
	[[ -n "$cli_library" ]] || fail 'CLI11.lib was not found in existing dependencies.'
	[[ -n "$llvm_library" ]] || fail 'LLVM-C.lib was not found. Set LLVM_ROOT to its existing installation.'
	[[ -n "$llvm_dll" ]] || fail 'LLVM-C.dll was not found in existing dependencies or distro/windows-x64.'

	copy_directory_contents "$cli_include" "$dependency_dir/include/CLI"
	copy_directory_contents "$json_include" "$dependency_dir/include/nlohmann"
	copy_file "$cli_library" "$dependency_dir/lib/CLI11.lib"
	copy_file "$llvm_library" "$dependency_dir/lib/LLVM-C.lib"
	copy_file "$llvm_dll" "$dependency_dir/bin/LLVM-C.dll"
	if [[ -f "$root/distro/windows-x64/vcruntime140.dll" ]]; then
		copy_file "$root/distro/windows-x64/vcruntime140.dll" "$native_dir/vcruntime140.dll"
	fi
	if [[ -f "$root/distro/windows-x64/vcruntime140_1.dll" ]]; then
		copy_file "$root/distro/windows-x64/vcruntime140_1.dll" "$native_dir/vcruntime140_1.dll"
	fi
	if [[ -f "$root/distro/windows-x64/msvcp140.dll" ]]; then
		copy_file "$root/distro/windows-x64/msvcp140.dll" "$native_dir/msvcp140.dll"
	fi
	copy_file "$dependency_dir/bin/LLVM-C.dll" "$native_dir/LLVM-C.dll"
}

write_toolchain_files() {
	local version
	version="$(tr -d '\r\n' < "$version_file")"
	[[ "$version" =~ ^0\.0\.[1-9][0-9]*$ ]] || fail "VERSION has an invalid Rutile version: $version"
	local installation
	installation="$(find_vs_installation)"
	local vsdevcmd="$installation/Common7/Tools/VsDevCmd.bat"
	[[ -f "$vsdevcmd" ]] || fail "Visual Studio environment setup was not found: $vsdevcmd"
	local vsdevcmd_win
	vsdevcmd_win="$(cygpath -w "$vsdevcmd")"
	local config_win
	config_win="$(cygpath -w "$toolchain_dir/config.hpp")"
	local native_win
	native_win="$(cygpath -w "$native_dir")"
	local dependency_lib_win
	dependency_lib_win="$(cygpath -w "$dependency_dir/lib")"
	local language_object_win
	language_object_win="$(cygpath -w "$native_dir/objects/rutile-language.obj")"
	local windows_object_win
	windows_object_win="$(cygpath -w "$native_dir/objects/windows.obj")"
	local runtime_object_win
	runtime_object_win="$(cygpath -w "$native_dir/objects/runtime.obj")"
	local version_object_win
	version_object_win="$(cygpath -w "$native_dir/objects/version.obj")"
	local main_object_win
	main_object_win="$(cygpath -w "$native_dir/objects/main.obj")"
	local project_sources_object_win
	project_sources_object_win="$(cygpath -w "$native_dir/objects/project_sources.obj")"
	local service_object_win
	service_object_win="$(cygpath -w "$native_dir/objects/service.obj")"
	mkdir -p "$toolchain_dir" "$native_dir/objects"
	{
		printf '#pragma once\n'
		printf '#define RUTILE_VERSION "%s"\n' "$version"
		printf '#define RUTILE_PROTOCOL_VERSION 2\n'
	} > "$toolchain_dir/config.hpp"
	local link_profile
	for link_profile in debug release; do
		local response_file="$native_dir/link-$link_profile.rsp"
		{
			printf '/nologo\n/machine:x64\n'
			if [[ "$link_profile" == debug ]]; then
				printf '/DEBUG\n'
			fi
			printf '/out:"%s\\rutilec.exe"\n' "$native_win"
			printf '/libpath:"%s"\n' "$dependency_lib_win"
			printf '"%s"\n' "$language_object_win"
			printf '"%s"\n' "$windows_object_win"
			printf '"%s"\n' "$runtime_object_win"
			printf '"%s"\n' "$version_object_win"
			printf '"%s"\n' "$main_object_win"
			printf '"%s"\n' "$project_sources_object_win"
			printf '"%s"\n' "$service_object_win"
			printf '%s\n' LLVM-C.lib CLI11.lib legacy_stdio_definitions.lib kernel32.lib user32.lib shell32.lib advapi32.lib
		} > "$response_file"
	done

	local batch="$native_dir/build.cmd"
	: > "$batch"
	cmd_line() {
		printf '%s\r\n' "$1" >> "$batch"
	}
	cmd_line '@echo off'
	cmd_line 'setlocal'
	cmd_line 'cd /d "%~dp0\..\..\.."'
	cmd_line 'if errorlevel 1 exit /b %errorlevel%'
	cmd_line 'set "RUTILE_NATIVE_CFLAGS=/Od /Zi"'
	cmd_line 'set "RUTILE_NATIVE_LDFLAGS=/DEBUG"'
	cmd_line 'if /I "%RUTILE_PROFILE%"=="release" set "RUTILE_NATIVE_CFLAGS=/O2"'
	cmd_line 'if /I "%RUTILE_PROFILE%"=="release" set "RUTILE_NATIVE_LDFLAGS="'
	cmd_line "call \"$vsdevcmd_win\" -arch=x64 -host_arch=x64"
	cmd_line 'if errorlevel 1 exit /b %errorlevel%'
	cmd_line 'cd /d "%~dp0\..\..\.."'
	cmd_line 'if errorlevel 1 exit /b %errorlevel%'
	cmd_line 'if not exist "build\native\objects" mkdir "build\native\objects"'
	cmd_line "cl /nologo /std:c++latest /EHsc /MD %RUTILE_NATIVE_CFLAGS% /I\"base\\include\" /I\"compiler\\include\" /I\"build\\dependencies\\include\" /FI\"$config_win\" /c \"base\\src\\windows.cpp\" /Fo\"build\\native\\objects\\windows.obj\""
	cmd_line 'if errorlevel 1 exit /b %errorlevel%'
	cmd_line "cl /nologo /std:c++latest /EHsc /MD %RUTILE_NATIVE_CFLAGS% /I\"base\\include\" /I\"compiler\\include\" /I\"build\\dependencies\\include\" /FI\"$config_win\" /c \"base\\src\\runtime.cpp\" /Fo\"build\\native\\objects\\runtime.obj\""
	cmd_line 'if errorlevel 1 exit /b %errorlevel%'
	cmd_line "cl /nologo /std:c++latest /EHsc /MD %RUTILE_NATIVE_CFLAGS% /I\"base\\include\" /I\"compiler\\include\" /I\"build\\dependencies\\include\" /FI\"$config_win\" /c \"base\\src\\version.cpp\" /Fo\"build\\native\\objects\\version.obj\""
	cmd_line 'if errorlevel 1 exit /b %errorlevel%'
	cmd_line "cl /nologo /std:c++latest /EHsc /MD %RUTILE_NATIVE_CFLAGS% /I\"base\\include\" /I\"compiler\\include\" /I\"build\\dependencies\\include\" /FI\"$config_win\" /c \"compiler\\src\\main.cpp\" /Fo\"build\\native\\objects\\main.obj\""
	cmd_line 'if errorlevel 1 exit /b %errorlevel%'
	cmd_line "cl /nologo /std:c++latest /EHsc /MD %RUTILE_NATIVE_CFLAGS% /I\"base\\include\" /I\"compiler\\include\" /I\"build\\dependencies\\include\" /FI\"$config_win\" /c \"compiler\\src\\project_sources.cpp\" /Fo\"build\\native\\objects\\project_sources.obj\""
	cmd_line 'if errorlevel 1 exit /b %errorlevel%'
	cmd_line "cl /nologo /std:c++latest /EHsc /MD %RUTILE_NATIVE_CFLAGS% /I\"base\\include\" /I\"compiler\\include\" /I\"build\\dependencies\\include\" /FI\"$config_win\" /c \"compiler\\src\\service.cpp\" /Fo\"build\\native\\objects\\service.obj\""
	cmd_line 'if errorlevel 1 exit /b %errorlevel%'
	cmd_line 'copy /y "build\native\rutile-language.obj" "build\native\objects\rutile-language.obj"'
	cmd_line 'if errorlevel 1 exit /b %errorlevel%'
	cmd_line 'link @build\native\link-%RUTILE_PROFILE%.rsp > build\native\link.log 2>&1'
	cmd_line 'set RUTILE_LINK_STATUS=%errorlevel%'
	cmd_line 'type build\native\link.log'
	cmd_line 'if not "%RUTILE_LINK_STATUS%"=="0" exit /b %RUTILE_LINK_STATUS%'
	cmd_line 'if errorlevel 1 exit /b %errorlevel%'
	# The bootstrap batch uses paths relative to its selected profile.
	sed -i "s|build\\\\native|build\\\\$profile\\\\native|g; s|build\\\\dependencies|build\\\\$profile\\\\dependencies|g" "$batch"
}

prepare() {
	[[ -f "$version_file" ]] || fail "Missing version source: $version_file"
	prepare_dependencies
	write_toolchain_files
	printf 'Prepared native Rutile toolchain in %s.\n' "$build_dir"
}

find_seed_compiler() {
	local compiler="${RUTILE_COMPILER:-}"
	if [[ -z "$compiler" ]]; then
		compiler="$(command -v rutilec || true)"
	fi
	if [[ -z "$compiler" ]]; then
		compiler="$root/distro/windows-x64/rutilec.exe"
	fi
	compiler="$(to_unix_path "$compiler")"
	[[ -f "$compiler" ]] || fail "Rutilec seed not found: $compiler. Set RUTILE_COMPILER or add rutilec to PATH."
	printf '%s\n' "$compiler"
}

build_compiler() {
	local profile=$1
	local optimization=0
	if [[ "$profile" == release ]]; then
		optimization=2
	fi
	prepare
	local compiler
	compiler="$(find_seed_compiler)"
	export PATH="$(dirname -- "$compiler"):$root/distro/windows-x64:$PATH"
	local -a sources=()
	local name
	for name in unique optional buffer string_view string hash equal unordered_map json identifier; do
		if [[ -f "$root/compiler/std/$name.rt" ]]; then
			sources+=( "compiler/std/$name.rt" )
		fi
	done
	local source
	while IFS= read -r source; do
		local already_added=0
		for name in unique optional buffer string_view string hash equal unordered_map json identifier; do
			if [[ "$source" == "compiler/std/$name.rt" ]]; then
				already_added=1
				break
			fi
		done
		if [[ "$already_added" == 0 ]]; then
			sources+=( "$source" )
		fi
	done < <(find compiler/std -type f -name '*.rt' -print | LC_ALL=C sort)
	while IFS= read -r source; do
		sources+=( "$source" )
	done < <(find compiler/src language -type f -name '*.rt' -print | LC_ALL=C sort)
	mkdir -p "$native_dir"
	printf 'Compiling %s Rutile files with %s (%s, optimization %s).\n' "${#sources[@]}" "$compiler" "$profile" "$optimization"
	"$compiler" "${sources[@]}" --output "build/$profile/native/rutile-language.obj" --optimization "$optimization"
	local batch_win
	batch_win="$(cygpath -w "$native_dir/build.cmd")"
	export RUTILE_PROFILE="$profile"
	MSYS2_ARG_CONV_EXCL='*' cmd.exe /d /c "$batch_win"
	printf 'Built %s\n' "$native_dir/rutilec.exe"
}

if [[ ! -f "$version_file" ]]; then
	fail "Missing version source: $version_file"
fi

operation="${1:-}"
if [[ $# -gt 0 ]]; then
	shift
fi
case "$operation" in
	prepare)
		[[ $# -le 1 ]] || fail 'Usage: tools/build.sh prepare [debug|release]'
		prepare
		;;
	build)
		profile="${1:-debug}"
		[[ $# -le 1 ]] || fail 'Usage: tools/build.sh build [debug|release]'
		case "$profile" in
			debug|release) ;;
			*) fail 'Profile must be debug or release.' ;;
		esac
		build_compiler "$profile"
		;;
	help|-h|--help)
		[[ $# -eq 0 ]] || fail 'Usage: tools/build.sh help'
		usage
		;;
	*)
		usage >&2
		fail 'An operation is required.'
		;;
esac
