$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$output = Join-Path $root 'build/vsix-tests/highlighting'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VSSDK -property installationPath
$msbuild = Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
$compiler = Join-Path $installation 'MSBuild/Current/Bin/Roslyn/csc.exe'
& $msbuild (Join-Path $PSScriptRoot '../Rutile.VisualStudio.csproj') /t:Build /p:Configuration=Release /p:CreateVsixContainer=false /p:DeployExtension=false /nologo /v:minimal *> (Join-Path $output 'build.log')
if ($LASTEXITCODE) {
    throw ('Extension SDK compilation failed: ' + (Join-Path $output 'build.log'))
}
$assembly = Join-Path $PSScriptRoot '../bin/Release/net472'
$client = Join-Path $assembly 'Microsoft.VisualStudio.LanguageServer.Client.dll'
$json = Join-Path $assembly 'Newtonsoft.Json.dll'
$executable = Join-Path $output 'HighlightingTests.exe'
& $compiler /nologo /langversion:latest /nullable:enable /target:exe "/out:$executable" "/reference:$client" "/reference:$json" `
    (Join-Path $PSScriptRoot '../RutileLexicalScanner.cs') `
    (Join-Path $PSScriptRoot '../RutileSemanticColors.cs') `
    (Join-Path $PSScriptRoot '../RutileColorNames.cs') `
    (Join-Path $PSScriptRoot '../RutileSemanticDecoder.cs') `
    (Join-Path $PSScriptRoot '../RutileDocumentText.cs') `
    (Join-Path $PSScriptRoot 'HighlightingTests.cs')
if ($LASTEXITCODE) {
    throw 'Highlighting test compilation failed.'
}
Copy-Item -LiteralPath $client,$json -Destination $output -Force
& $executable
if ($LASTEXITCODE) {
    throw 'Highlighting tests failed.'
}
