$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$output = Join-Path $root 'build/vsix-tests/kiln-workspace'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VSSDK -property installationPath
$compiler = Join-Path $installation 'MSBuild/Current/Bin/Roslyn/csc.exe'
$json = Join-Path $env:USERPROFILE '.nuget/packages/newtonsoft.json/13.0.1/lib/net45/Newtonsoft.Json.dll'
Copy-Item -LiteralPath $json -Destination $output
$executable = Join-Path $output 'KilnWorkspaceTests.exe'
& $compiler /nologo /langversion:latest /nullable:enable /target:exe "/out:$executable" "/reference:$json" `
    (Join-Path $PSScriptRoot '../KilnProject.cs') `
    (Join-Path $PSScriptRoot '../KilnWorkspaceFiles.cs') `
    (Join-Path $PSScriptRoot 'KilnWorkspaceTests.cs')
if ($LASTEXITCODE) {
    throw 'Kiln workspace test compilation failed.'
}
& $executable (Join-Path $output ('project-' + [Guid]::NewGuid().ToString('N')))
if ($LASTEXITCODE) {
    throw 'Kiln workspace tests failed.'
}
