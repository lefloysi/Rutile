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
$kiln = [IO.Path]::GetFullPath((Join-Path $root '../Kiln'))
$prepared = Join-Path $kiln 'build/bootstrap'
$arguments = @('/nologo', '/std:c++latest', '/EHsc', '/MD',
	('/FI"' + $prepared + '/config.hpp"'),
	('/I"' + $root + '/../Kiln/base/include"'), ('/I"' + $root + '/../Kiln/src"'),
	('/I"' + $root + '/../Kiln/project"'), ('/I"' + $prepared + '/include"'),
	('/I"' + $prepared + '"'),
	('"' + $root + '/tests/native/compiler_integrations.cpp"'),
	('"' + $root + '/../Kiln/src/compiler.cpp"'),
	('"' + $root + '/../Kiln/project/configuration.cpp"'),
	('"' + $root + '/../Kiln/project/sources.cpp"'),
	('"' + $root + '/../Kiln/base/src/windows.cpp"'),
	'/Fe:case.exe', '/link', 'kernel32.lib', 'user32.lib', 'shell32.lib', 'advapi32.lib')
$command = '"' + $dev + '" -no_logo -arch=x64 && cl ' + ($arguments -join ' ')
$previous_path = $env:PATH
Push-Location $output
try {
	& cmd /d /s /c $command
	if ($LASTEXITCODE) {
		throw 'Compiler integration test compilation failed.'
	}
	$env:PATH = (Join-Path $prepared 'bin') + ';' + $env:PATH
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
