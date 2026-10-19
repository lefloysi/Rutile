$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$output = Join-Path $root 'build/vsix-tests/package-startup'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VSSDK -property installationPath
$compiler = Join-Path $installation 'MSBuild/Current/Bin/Roslyn/csc.exe'
$executable = Join-Path $output 'PackageStartupTests.exe'
& $compiler /nologo /langversion:latest /nullable:enable /target:exe "/out:$executable" `
    (Join-Path $PSScriptRoot '../KilnPackage.cs') `
    (Join-Path $PSScriptRoot 'PackageShell.cs') `
    (Join-Path $PSScriptRoot 'PackageStartupTests.cs')
if ($LASTEXITCODE) {
    throw 'Package startup test compilation failed.'
}
& $executable
if ($LASTEXITCODE) {
    throw 'Package startup regression tests failed.'
}
