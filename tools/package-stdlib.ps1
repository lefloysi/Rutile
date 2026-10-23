param(
	[Parameter(Mandatory = $true)][string]$Compiler,
	[string]$Destination = "",
	[int]$Optimization = 2
)
$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$compilerPath = (Resolve-Path -LiteralPath $Compiler).Path
if (!$Destination) {
	$Destination = Join-Path $projectRoot "build/release/stdlib"
}
$destinationPath = [IO.Path]::GetFullPath($Destination)
[IO.Directory]::CreateDirectory($destinationPath) | Out-Null
$objectPath = Join-Path $destinationPath "std.obj"
$interfacePath = Join-Path $destinationPath "std.rti"
$libraryPath = Join-Path $destinationPath "std.lib"
$sources = @(Get-ChildItem -LiteralPath (Join-Path $projectRoot "compiler/std") -Filter "*.rt" -File -Recurse | Sort-Object FullName | ForEach-Object FullName)
& $compilerPath @sources --module std --kind library --optimization $Optimization --output $objectPath --interface-output $interfacePath
if ($LASTEXITCODE) {
	throw "Standard library compilation failed."
}
$archiver = Get-Command "lib.exe" -ErrorAction SilentlyContinue
if (!$archiver) {
	$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
	$installation = & $vswhere -latest -products "*" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
	$toolset = Get-ChildItem -LiteralPath (Join-Path $installation "VC/Tools/MSVC") -Directory | Sort-Object Name -Descending | Select-Object -First 1
	$archiverPath = Join-Path $toolset.FullName "bin/Hostx64/x64/lib.exe"
}
else {
	$archiverPath = $archiver.Source
}
& $archiverPath /nologo /machine:x64 "/out:$libraryPath" $objectPath
if ($LASTEXITCODE) {
	throw "Standard library archiving failed."
}
$metadata = Get-Content -LiteralPath $interfacePath -Raw | ConvertFrom-Json
Write-Output "Standard library $($metadata.compiler): $libraryPath, $interfacePath"
