#!/usr/bin/env bash
set -euo pipefail

name= version= payload= output=
while (($#)); do
	case "$1" in
		--name|--version|--payload|--output)
			(($# >= 2)) || { echo "Missing value for $1" >&2; exit 1; }
			case "$1" in
				--name) name=$2 ;;
				--version) version=$2 ;;
				--payload) payload=$2 ;;
				--output) output=$2 ;;
			esac
			shift 2 ;;
		*) echo "Unknown option: $1" >&2; exit 1 ;;
	esac
done
[[ $name == Kiln || $name == Rutile ]] || { echo '--name must be Kiln or Rutile' >&2; exit 1; }
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo 'Invalid --version' >&2; exit 1; }
[[ -d $payload && -n $output ]] || { echo '--payload and --output are required' >&2; exit 1; }
tool=kiln
other=rutilec
if [[ $name == Rutile ]]; then
	tool=rutilec
	other=kiln
fi
[[ -f $payload/bin/$tool.exe ]] || { echo "Missing bin/$tool.exe in payload" >&2; exit 1; }
if [[ $name == Rutile ]]; then
	[[ -f $payload/share/rutile/lib/std.lib && -f $payload/share/rutile/lib/std.rti ]] || { echo 'Rutile payload requires version-matched std.lib and std.rti' >&2; exit 1; }
fi
[[ ! -e $payload/bin/$other.exe ]] || { echo 'Independent installers must not bundle the other tool' >&2; exit 1; }
source_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
mkdir -p -- "$(dirname -- "$output")"
output_dir=$(cd -- "$(dirname -- "$output")" && pwd)
output="$output_dir/$(basename -- "$output")"
payload=$(cd -- "$payload" && pwd)
work=$(mktemp -d "$output_dir/.installer-build.XXXXXXXX")
cleanup() {
	case "$work" in
		"$output_dir"/.installer-build.*) rm -rf -- "$work" ;;
		*) echo "Refusing to remove unexpected build directory: $work" >&2 ;;
	esac
}
trap cleanup EXIT
csc=${CSC:-}
if [[ -z $csc ]]; then
	for candidate in "/c/Program Files/Microsoft Visual Studio/2022"/*/MSBuild/Current/Bin/Roslyn/csc.exe "/mnt/c/Program Files/Microsoft Visual Studio/2022"/*/MSBuild/Current/Bin/Roslyn/csc.exe /c/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe /mnt/c/Windows/Microsoft.NET/Framework64/v4.0.30319/csc.exe; do
		if [[ -f $candidate ]]; then
			csc=$candidate
			break
		fi
	done
fi
[[ -n $csc && -f $csc ]] || { echo 'Set CSC to the Windows C# compiler csc.exe' >&2; exit 1; }
winpath() {
	if command -v cygpath >/dev/null 2>&1; then cygpath -w -- "$1"; else wslpath -w -- "$1"; fi
}
# Disable MSYS argument conversion: csc options start with '/', and all paths below
# are converted explicitly so spaces and resource paths survive unchanged.
MSYS2_ARG_CONV_EXCL='*' "$csc" /nologo /target:exe /optimize+ /platform:anycpu \
	"/out:$(winpath "$work/Pack.exe")" /reference:System.IO.Compression.dll \
	/reference:System.IO.Compression.FileSystem.dll "$(winpath "$source_dir/Pack.cs")"
"$work/Pack.exe" "$(winpath "$payload")" "$(winpath "$work/payload.zip")"
printf '%s\n%s\n' "$name" "$version" > "$work/product.txt"
MSYS2_ARG_CONV_EXCL='*' "$csc" /nologo /target:exe /optimize+ /platform:anycpu \
	"/out:$(winpath "$output")" /reference:System.IO.Compression.dll \
	/reference:System.IO.Compression.FileSystem.dll \
	"/resource:$(winpath "$work/payload.zip"),payload.zip" \
	"/resource:$(winpath "$work/product.txt"),product.txt" \
	"$(winpath "$source_dir/Setup.cs")"
echo "$output"
