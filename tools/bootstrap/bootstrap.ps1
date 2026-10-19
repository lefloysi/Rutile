[CmdletBinding()]
param(
	[string]$Dependencies,
	[string]$Llvm,
	[ValidateSet('debug', 'release')][string]$Profile = 'debug',
	[switch]$PrepareOnly,
	[switch]$ReuseLanguageObject
)
$ErrorActionPreference = 'Stop'
$project_root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$build_root = Join-Path $project_root 'build/bootstrap'
New-Item -ItemType Directory -Force $build_root | Out-Null

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
	& $Program @Arguments
	if ($LASTEXITCODE -ne 0) {
		throw "$Program failed with exit code $LASTEXITCODE."
	}
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) {
	throw 'Install Visual Studio with the Desktop development with C++ workload.'
}
$visual_studio = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$visual_studio) {
	throw 'Visual Studio C++ tools were not found.'
}
$developer_command = Join-Path $visual_studio 'Common7/Tools/VsDevCmd.bat'
$environment_command = 'call "' + $developer_command + '" -no_logo -arch=x64 >nul && set'
$environment_names = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
& $env:ComSpec /d /c $environment_command | ForEach-Object {
	if ($_ -match '^([^=]+)=(.*)$' -and $environment_names.Add($Matches[1])) {
		[Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process')
	}
}
if ($LASTEXITCODE -ne 0) {
	throw 'Could not initialize the Visual Studio C++ environment.'
}
$compiler = (Get-Command cl.exe -ErrorAction Stop).Source
$linker = (Get-Command link.exe -ErrorAction Stop).Source
$archiver = (Get-Command lib.exe -ErrorAction Stop).Source

if (!$Dependencies) {
	$candidates = @(
		(Join-Path $project_root 'build/bootstrap/vcpkg_installed/x64-windows'),
		(Join-Path $project_root 'vcpkg_installed/x64-windows'),
		(Join-Path $project_root 'build/debug/vcpkg_installed/x64-windows')
	)
	if ($env:VCPKG_ROOT) {
		$candidates += Join-Path $env:VCPKG_ROOT 'installed/x64-windows'
	}
	$Dependencies = $candidates | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'include/CLI/CLI.hpp') } | Select-Object -First 1
}
if (!$Dependencies) {
	if (!$env:VCPKG_ROOT) {
		throw 'Set VCPKG_ROOT or pass -Dependencies with an installed x64-windows dependency directory.'
	}
	$vcpkg = Join-Path $env:VCPKG_ROOT 'vcpkg.exe'
	Invoke-Checked $vcpkg @('install', '--triplet=x64-windows', "--x-manifest-root=$project_root", "--x-install-root=$build_root/vcpkg_installed")
	$Dependencies = Join-Path $build_root 'vcpkg_installed/x64-windows'
}
$Dependencies = (Resolve-Path -LiteralPath $Dependencies).Path
foreach ($header in @('CLI/CLI.hpp', 'nlohmann/json.hpp')) {
	if (!(Test-Path -LiteralPath (Join-Path $Dependencies "include/$header"))) {
		throw "Missing dependency header $header in $Dependencies."
	}
}
New-Item -ItemType Directory -Force "$build_root/include", "$build_root/lib", "$build_root/bin" | Out-Null
foreach ($include in @('CLI', 'nlohmann', 'catch2')) {
	Copy-Item -LiteralPath (Join-Path $Dependencies "include/$include") -Destination "$build_root/include" -Recurse -Force
}
foreach ($library in @('CLI11.lib', 'Catch2.lib')) {
	Copy-Item -LiteralPath (Join-Path $Dependencies "lib/$library") -Destination "$build_root/lib/$library" -Force
}
Copy-Item -LiteralPath (Join-Path $Dependencies 'lib/manual-link/Catch2Main.lib') -Destination "$build_root/lib/Catch2Main.lib" -Force


if (!$Llvm) {
	$Llvm = Join-Path $project_root 'distro/windows-x64/LLVM-C.dll'
}
if (Test-Path -LiteralPath $Llvm -PathType Container) {
	$Llvm = Join-Path $Llvm 'bin/LLVM-C.dll'
}
$Llvm = (Resolve-Path -LiteralPath $Llvm).Path
Copy-Item -LiteralPath $Llvm -Destination "$build_root/bin/LLVM-C.dll" -Force
$exports = & (Get-Command dumpbin.exe).Source /nologo /exports $Llvm
if ($LASTEXITCODE -ne 0) {
	throw 'Could not read LLVM C API exports.'
}
$symbols = @($exports | ForEach-Object {
	if ($_ -match '^\s+\d+\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(LLVM\S+)') {
		$Matches[1]
	}
})
if (!$symbols.Count) {
	throw 'The LLVM DLL does not export the LLVM C API.'
}
@('LIBRARY LLVM-C.dll', 'EXPORTS') + $symbols | Set-Content -LiteralPath "$build_root/LLVM-C.def" -Encoding ascii
Invoke-Checked $archiver @('/nologo', '/machine:x64', "/def:$build_root/LLVM-C.def", "/out:$build_root/lib/LLVM-C.lib")

function Cpp-String([string]$Value) {
	return '"' + $Value.Replace('\', '/').Replace('"', '\"') + '"'
}
$version = (Get-Content -LiteralPath "$project_root/version/release.txt" -Raw).Trim()
$macros = [ordered]@{
	RUTILE_VERSION = Cpp-String $version
	RUTILE_PROTOCOL_VERSION = '1'
	RUTILE_SOURCE_ROOT = Cpp-String $project_root
	KILN_TEST_EXECUTABLE = Cpp-String "$build_root/bin/kiln.exe"
	KILN_TEST_ROOT = Cpp-String "$project_root/build/tests/kiln"
	KILN_EXECUTABLE_SUFFIX = '".exe"'
	KILN_EXECUTABLE_NAME = '"kiln.exe"'
	KILN_CONFIGURATION_STDLIB = Cpp-String "$project_root/kiln/compiler/std"
	KILN_CONFIGURATION_API = Cpp-String "$project_root/kiln/configuration/api.rt"
	RUTILE_NATIVE_LINKER = Cpp-String $linker
	RUTILE_RUNTIME_LIBCMT = Cpp-String "$env:VCToolsInstallDir/lib/x64/libcmt.lib"
	RUTILE_RUNTIME_LIBVCRUNTIME = Cpp-String "$env:VCToolsInstallDir/lib/x64/libvcruntime.lib"
	RUTILE_RUNTIME_OLDNAMES = Cpp-String "$env:VCToolsInstallDir/lib/x64/oldnames.lib"
	RUTILE_RUNTIME_LIBUCRT = Cpp-String "$env:UniversalCRTSdkDir/Lib/$env:UCRTVersion/ucrt/x64/libucrt.lib"
	RUTILE_RUNTIME_KERNEL32 = Cpp-String "$env:WindowsSdkDir/Lib/$env:WindowsSDKVersion/um/x64/kernel32.lib"
}
$header = @('#pragma once')
foreach ($macro in $macros.GetEnumerator()) {
	$header += "#define $($macro.Key) $($macro.Value)"
}
$header | Set-Content -LiteralPath "$build_root/config.hpp" -Encoding ascii
if ($PrepareOnly) {
	Write-Output "Prepared native dependencies and toolchain in $build_root."
	return
}

$seed = Join-Path $project_root 'distro/windows-x64/compiler.exe'
$language_sources = @('unique', 'optional', 'buffer', 'string_view', 'string', 'hash', 'equal', 'unordered_map') | ForEach-Object {
	Join-Path $project_root "kiln/compiler/std/$_.rt"
}
$language_sources += Join-Path $project_root 'kiln/compiler/std/json.rt'
foreach ($folder in @('kiln/compiler/src', 'kiln/src')) {
	$language_sources += Get-ChildItem -LiteralPath (Join-Path $project_root $folder) -Recurse -Filter '*.rt' | Sort-Object FullName | Select-Object -ExpandProperty FullName
}
$optimization = if ($Profile -eq 'release') { '2' } else { '0' }
if (!$ReuseLanguageObject) {
	Write-Output 'Bootstrapping the integrated Rutile compiler and language service.'
	Invoke-Checked $seed ($language_sources + @('--output', "$build_root/language.obj", '--optimization', $optimization))
}
elseif (!(Test-Path -LiteralPath "$build_root/language.obj")) {
	throw 'No language object exists to reuse. Run bootstrap without -ReuseLanguageObject.'
}

$native_sources = @()
foreach ($folder in @('base/src', 'version', 'kiln/src', 'kiln/project', 'kiln/integrations', 'kiln/configuration')) {
	$native_sources += Get-ChildItem -LiteralPath (Join-Path $project_root $folder) -Filter '*.cpp' -Recurse |
		Where-Object { $_.Name -ne 'posix.cpp' } | Select-Object -ExpandProperty FullName
}
$objects = @("$build_root/language.obj")
$index = 0
Write-Output 'Building Kiln with the native C++ toolchain.'
foreach ($source in $native_sources) {
	$object = "$build_root/native-$index.obj"
	Invoke-Checked $compiler @('/nologo', '/std:c++latest', '/EHsc', '/MD', "/I$project_root/base/include", "/I$project_root/version", "/I$project_root/kiln/project", "/I$build_root/include", "/FI$build_root/config.hpp", '/c', $source, "/Fo$object")
	$objects += $object
	$index++
}
$link_arguments = @('/nologo', '/incremental:no', "/out:$build_root/bin/kiln.exe") + $objects + @(
	"$build_root/lib/LLVM-C.lib", "$build_root/lib/CLI11.lib",
	'legacy_stdio_definitions.lib', 'kernel32.lib', 'user32.lib', 'shell32.lib', 'advapi32.lib'
)
Invoke-Checked $linker $link_arguments
Write-Output "Kiln is ready: $build_root/bin/kiln.exe"
Write-Output 'Open this folder in Visual Studio, or run build/bootstrap/bin/kiln.exe build kiln.exe.'
