$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$prepared = [IO.Path]::GetFullPath((Join-Path $root '../Kiln/build/bootstrap'))
$output = Join-Path $root 'build/native-tests/compiler-progress'
New-Item -ItemType Directory -Force $output | Out-Null
$sources = Get-ChildItem "$root/compiler/std", "$root/compiler/src" -Recurse -Filter '*.rt' |
	Sort-Object FullName | Select-Object -ExpandProperty FullName
& "$root/distro/windows-x64/compiler.exe" @sources --output "$output/compiler.obj" --optimization 0
if ($LASTEXITCODE) {
	throw 'Compiler progress source compilation failed.'
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) {
	throw 'Visual Studio C++ tools are required.'
}
$dev = Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
$arguments = @('/nologo', '/std:c++latest', '/EHsc', '/MD',
	('/I"' + $root + '/base/include"'),
	('"' + $root + '/tests/native/compiler_progress.cpp"'),
	('"' + $root + '/base/src/windows.cpp"'),
	('"' + $root + '/base/src/runtime.cpp"'),
	'compiler.obj', '/Fe:case.exe', '/link',
	('"' + $prepared + '/lib/LLVM-C.lib"'),
	'legacy_stdio_definitions.lib', 'kernel32.lib', 'user32.lib', 'shell32.lib', 'advapi32.lib')
$command = '"' + $dev + '" -no_logo -arch=x64 && cl ' + ($arguments -join ' ')
$previous_path = $env:PATH
Push-Location $output
try {
	& cmd /d /s /c $command
	if ($LASTEXITCODE) {
		throw 'Compiler progress test compilation failed.'
	}
	$env:PATH = (Join-Path $prepared 'bin') + ';' + $env:PATH
	& ./case.exe (Join-Path $output 'outputs')
	if ($LASTEXITCODE) {
		throw "Compiler progress test failed: $LASTEXITCODE"
	}
}
finally {
	$env:PATH = $previous_path
	Pop-Location
}
