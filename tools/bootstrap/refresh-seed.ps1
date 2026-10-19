[CmdletBinding()]
param(
	[string]$Kiln = "$PSScriptRoot/../../build/bootstrap/bin/kiln.exe",
	[string]$Dependencies
)
$ErrorActionPreference = 'Stop'
$project_root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$destination = Join-Path $project_root 'build/seed-refresh'
New-Item -ItemType Directory -Force "$destination/bin" | Out-Null
& "$PSScriptRoot/bootstrap.ps1" -PrepareOnly -Dependencies $Dependencies
$Kiln = (Resolve-Path -LiteralPath $Kiln).Path

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
	& $Program @Arguments
	if ($LASTEXITCODE -ne 0) {
		throw "$Program failed with exit code $LASTEXITCODE."
	}
}

$sources = @('unique', 'optional', 'buffer', 'string_view', 'string', 'hash', 'equal', 'unordered_map') | ForEach-Object {
	Join-Path $project_root "kiln/compiler/std/$_.rt"
}
$sources += Get-ChildItem -LiteralPath "$project_root/kiln/compiler/src" -Recurse -Filter '*.rt' |
	Sort-Object FullName | Select-Object -ExpandProperty FullName
$compiler_object = "$destination/compiler.obj"
Invoke-Checked $Kiln (@('compile') + $sources + @('--output', $compiler_object, '--optimization', '2'))

$objects = @($compiler_object)
$index = 0
foreach ($source in @('kiln/compiler/src/main.cpp', 'base/src/windows.cpp', 'base/src/runtime.cpp', 'version/runtime.cpp')) {
	$object = "$destination/seed-native-$index.obj"
	Invoke-Checked (Get-Command cl.exe).Source @('/nologo', '/std:c++latest', '/EHsc', '/MD', "/I$project_root/base/include", "/I$project_root/version", "/I$project_root/build/bootstrap/include", "/FI$project_root/build/bootstrap/config.hpp", '/c', "$project_root/$source", "/Fo$object")
	$objects += $object
	$index++
}
$seed = "$destination/bin/compiler.exe"
Invoke-Checked (Get-Command link.exe).Source (@('/nologo', '/incremental:no', "/out:$seed") + $objects + @(
	"$project_root/build/bootstrap/lib/LLVM-C.lib", "$project_root/build/bootstrap/lib/CLI11.lib",
	'legacy_stdio_definitions.lib', 'kernel32.lib', 'user32.lib', 'shell32.lib', 'advapi32.lib'
))
Copy-Item -LiteralPath "$project_root/distro/windows-x64/LLVM-C.dll" -Destination "$destination/bin/LLVM-C.dll" -Force

$parts = foreach ($source in $sources) {
	[IO.File]::ReadAllText($source).Replace("`r`n", "`n").TrimEnd("`n")
}
$snapshot = "$destination/compiler.rt"
[IO.File]::WriteAllText($snapshot, ($parts -join "`n`n") + "`n", [Text.UTF8Encoding]::new($false))
Invoke-Checked $seed ($sources + @('--output', "$destination/selfhost.obj", '--optimization', '2'))
Invoke-Checked $seed @($snapshot, '--output', "$destination/snapshot.obj", '--optimization', '2')
$expected = (Get-FileHash -LiteralPath $compiler_object -Algorithm SHA256).Hash
foreach ($object in @("$destination/selfhost.obj", "$destination/snapshot.obj")) {
	if ((Get-FileHash -LiteralPath $object -Algorithm SHA256).Hash -ne $expected) {
		throw "Bootstrap verification did not produce identical objects: $object."
	}
}
Copy-Item -LiteralPath $seed -Destination "$project_root/distro/windows-x64/compiler.exe" -Force
Copy-Item -LiteralPath $snapshot -Destination "$project_root/distro/compiler.rt" -Force
$version = [IO.File]::ReadAllText("$project_root/version/release.txt").Trim()
[IO.File]::WriteAllText("$project_root/distro/version.txt", $version + "`n", [Text.UTF8Encoding]::new($false))
$hash = (Get-FileHash -LiteralPath $snapshot -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$project_root/distro/source.sha256", $hash + "`n", [Text.UTF8Encoding]::new($false))
Write-Output "Refreshed verified private compiler seed $version."
