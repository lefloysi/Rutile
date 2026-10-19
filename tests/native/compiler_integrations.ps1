$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output = Join-Path $root 'build/native-tests/compiler-integrations'
New-Item -ItemType Directory -Force $output | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) {
	throw 'Visual Studio C++ tools are required.'
}
$dev = Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
$dependencies = Join-Path $env:USERPROFILE 'Documents/vcpkg/installed/x64-windows'
$arguments = @('/nologo', '/std:c++latest', '/EHsc', '/MD',
	('/I"' + $root + '/base/include"'), ('/I"' + $root + '/kiln/src"'),
	('/I"' + $root + '/kiln/project"'), ('/I"' + $dependencies + '/include"'),
	('/I"' + $root + '/build/debug/vcpkg_installed/x64-windows/include"'),
	'/DKILN_EXECUTABLE_SUFFIX=\".exe\"',
	('"' + $root + '/tests/native/compiler_integrations.cpp"'),
	('"' + $root + '/kiln/src/compiler.cpp"'),
	('"' + $root + '/kiln/project/configuration.cpp"'),
	('"' + $root + '/kiln/project/sources.cpp"'),
	('"' + $root + '/base/src/windows.cpp"'),
	('"' + $dependencies + '/lib/yaml-cpp.lib"'),
	'/Fe:case.exe', '/link', 'kernel32.lib', 'user32.lib', 'shell32.lib', 'advapi32.lib')
$command = '"' + $dev + '" -no_logo -arch=x64 && cl ' + ($arguments -join ' ')
$previous_path = $env:PATH
Push-Location $output
try {
	& cmd /d /s /c $command
	if ($LASTEXITCODE) {
		throw 'Compiler integration test compilation failed.'
	}
	$env:PATH = (Join-Path $dependencies 'bin') + ';' + $env:PATH
	& ./case.exe (Join-Path $output 'projects')
	if ($LASTEXITCODE -ne 42) {
		throw "Compiler integration test failed: $LASTEXITCODE"
	}
	Write-Output 'compiler-integrations: PASS'
}
finally {
	$env:PATH = $previous_path
	Pop-Location
}
