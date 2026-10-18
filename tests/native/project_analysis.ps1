param([ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug')

$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build = Join-Path $root ('build/' + $Configuration.ToLowerInvariant())
$output = Join-Path $root ('build/native-tests/project-analysis-' + $Configuration.ToLowerInvariant())
New-Item -ItemType Directory -Force $output | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) {
	throw 'Visual Studio C++ tools are required.'
}
$dev = Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
$dependencies = Join-Path $build 'vcpkg_installed/x64-windows'
$runtime = '/MD'
$libraries = Join-Path $dependencies 'lib'
$binaries = Join-Path $dependencies 'bin'
$yaml = 'yaml-cpp.lib'
if ($Configuration -eq 'Debug') {
	$runtime = '/MDd'
	$libraries = Join-Path $dependencies 'debug/lib'
	$binaries = Join-Path $dependencies 'debug/bin'
	$yaml = 'yaml-cppd.lib'
}
$arguments = @('/nologo', '/std:c++latest', '/EHsc', $runtime,
	('/I"' + $root + '/kiln/project"'), ('/I"' + $dependencies + '/include"'),
	('"' + $root + '/tests/native/project_analysis.cpp"'),
	('"' + $root + '/kiln/project/configuration.cpp"'),
	('"' + $root + '/kiln/project/sources.cpp"'),
	('"' + $build + '/rutile-project.lib"'), ('"' + $build + '/base/base.lib"'),
	('"' + $build + '/rutile-version.lib"'), ('"' + $libraries + '/' + $yaml + '"'),
	'/Fe:case.exe', '/link', 'kernel32.lib', 'user32.lib', 'shell32.lib', 'advapi32.lib')
$command = '"' + $dev + '" -no_logo -arch=x64 && cl ' + ($arguments -join ' ')
$previous_path = $env:PATH
Push-Location $output
try {
	& cmd /d /s /c $command
	if ($LASTEXITCODE) {
		throw 'Project analysis configuration test compilation failed.'
	}
	$env:PATH = $binaries + ';' + $env:PATH
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
