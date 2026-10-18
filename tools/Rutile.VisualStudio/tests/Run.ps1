$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$output = Join-Path $root 'build/vsix-tests/process'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VSSDK -property installationPath
$compiler = Join-Path $installation 'MSBuild/Current/Bin/Roslyn/csc.exe'
$executable = Join-Path $output 'ProcessTests.exe'
& $compiler /nologo /langversion:latest /nullable:enable /target:exe "/out:$executable" `
    (Join-Path $PSScriptRoot '../ServerProcess.cs') `
    (Join-Path $PSScriptRoot 'Connection.cs') (Join-Path $PSScriptRoot 'ProcessTests.cs')
if ($LASTEXITCODE) {
    throw 'Extension process test compilation failed.'
}
& $executable
if ($LASTEXITCODE) {
    throw 'Extension process tests failed.'
}
