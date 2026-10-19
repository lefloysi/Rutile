param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')

$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$kiln = [IO.Path]::GetFullPath((Join-Path $root '../Kiln'))
$prepared = Join-Path $kiln 'build/bootstrap'
$output = Join-Path $root ('build/native-tests/project-analysis-' + $Configuration.ToLowerInvariant())
New-Item -ItemType Directory -Force $output | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) {
	throw 'Visual Studio C++ tools are required.'
}
$dev = Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
$runtime = '/MD'
$arguments = @('/nologo', '/std:c++latest', '/EHsc', $runtime,
	('/FI"' + $prepared + '/config.hpp"'),
	('/I"' + $root + '/../Kiln/project"'), ('/I"' + $prepared + '/include"'), ('/I"' + $prepared + '"'),
	('/I"' + $kiln + '/base/include"'), ('/I"' + $kiln + '/version"'),
	('"' + $root + '/tests/native/project_analysis.cpp"'),
	('"' + $root + '/../Kiln/project/configuration.cpp"'),
	('"' + $root + '/../Kiln/project/sources.cpp"'),
	('"' + $kiln + '/project/session.cpp"'),
	('"' + $kiln + '/project/status.cpp"'),
	('"' + $kiln + '/base/src/windows.cpp"'),
	('"' + $kiln + '/version/runtime.cpp"'),
	'/Fe:case.exe', '/link', 'kernel32.lib', 'user32.lib', 'shell32.lib', 'advapi32.lib')
$command = '"' + $dev + '" -no_logo -arch=x64 && cl ' + ($arguments -join ' ')
$previous_path = $env:PATH
Push-Location $output
try {
	& cmd /d /s /c $command
	if ($LASTEXITCODE) {
		throw 'Project analysis configuration test compilation failed.'
	}
	$env:PATH = (Join-Path $prepared 'bin') + ';' + $env:PATH
	& ./case.exe (Join-Path $output 'project')
	if ($LASTEXITCODE -ne 42) {
		throw "Project analysis configuration test failed: $LASTEXITCODE"
	}
	Write-Output "project-analysis-$Configuration`: PASS"
}
finally {
	$env:PATH = $previous_path
	Pop-Location
}
