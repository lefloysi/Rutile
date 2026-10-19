$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output = Join-Path $root 'build/native-tests/editor-host'
New-Item -ItemType Directory -Force $output | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) {
	throw 'Visual Studio C++ tools are required.'
}
$dev = Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
$command = '"' + $dev + '" -no_logo -arch=x64 && cl /nologo /std:c++latest /EHsc /MD /I"' + $root + '/base/include" /I"' + $root + '/kiln/src" /I"' + $root + '/kiln/project" /I"' + $root + '/version" "' + $root + '/tests/native/editor_host.cpp" "' + $root + '/kiln/src/server.cpp" "' + $root + '/base/src/windows.cpp" /Fe:editor-host.exe'
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
