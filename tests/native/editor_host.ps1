$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$kiln = [IO.Path]::GetFullPath((Join-Path $root '../Kiln'))
$prepared = Join-Path $kiln 'build/bootstrap'
$output = Join-Path $root 'build/native-tests/editor-host'
New-Item -ItemType Directory -Force $output | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) {
	throw 'Visual Studio C++ tools are required.'
}
$dev = Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
$command = '"' + $dev + '" -no_logo -arch=x64 && cl /nologo /std:c++latest /EHsc /MD /I"' + $prepared + '/include" /I"' + $prepared + '" /I"' + $root + '/../Kiln/base/include" /I"' + $root + '/../Kiln/src" /I"' + $root + '/../Kiln/project" /I"' + $root + '/../Kiln/version" "' + $root + '/tests/native/editor_host.cpp" "' + $root + '/../Kiln/src/server.cpp" "' + $root + '/../Kiln/base/src/windows.cpp" /Fe:editor-host.exe'
Push-Location $output
try {
	& cmd /d /s /c $command
	if ($LASTEXITCODE) {
		throw 'Editor host test compilation failed.'
	}
	& ./editor-host.exe
	if ($LASTEXITCODE -ne 42) {
		throw "Editor host runtime failed: $LASTEXITCODE"
	}
	Write-Output 'editor-host: PASS'
}
finally {
	Pop-Location
}
