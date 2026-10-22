param([Parameter(Mandatory = $true)] [string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$output = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $output | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VSSDK -property installationPath
$compiler = Join-Path $installation 'MSBuild/Current/Bin/Roslyn/csc.exe'
$editor = Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/Editor'
$wpf = Join-Path $env:windir 'Microsoft.NET/Framework64/v4.0.30319/WPF'
$references = @('Microsoft.VisualStudio.CoreUtility.dll','Microsoft.VisualStudio.Text.Data.dll','Microsoft.VisualStudio.Text.Logic.dll','Microsoft.VisualStudio.Text.UI.dll','Microsoft.VisualStudio.Text.UI.Wpf.dll','Microsoft.VisualStudio.Editor.dll','Microsoft.VisualStudio.Language.dll') | ForEach-Object { '/reference:' + (Join-Path $editor $_) }
$references += @('WindowsBase.dll','PresentationCore.dll','PresentationFramework.dll') | ForEach-Object { '/reference:' + (Join-Path $wpf $_) }
$extension = Join-Path $PSScriptRoot '../bin/Release/net472'
$references += @('Microsoft.VisualStudio.Shell.15.0.dll','Microsoft.VisualStudio.Shell.Framework.dll','Microsoft.VisualStudio.Interop.dll','Microsoft.VisualStudio.ComponentModelHost.dll','Microsoft.VisualStudio.Threading.dll','Microsoft.VisualStudio.LanguageServer.Client.dll') | ForEach-Object { '/reference:' + (Join-Path $extension $_) }
$references += '/reference:' + (Join-Path $extension 'System.Collections.Immutable.dll')
$assembly = Join-Path $output 'Rutile.PaletteProbe.dll'
& $compiler /nologo /target:library /langversion:latest /nullable:enable /reference:System.ComponentModel.Composition.dll "/out:$assembly" @references (Join-Path $PSScriptRoot 'PaletteProbe.cs') *> (Join-Path $output 'compile.log')
if ($LASTEXITCODE) {
    throw ('Palette probe compilation failed: ' + (Join-Path $output 'compile.log'))
}
$manifest = Join-Path $output 'source.extension.vsixmanifest'
@'
<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0" xmlns="http://schemas.microsoft.com/developer/vsx-schema/2011">
  <Metadata><Identity Id="Rutile.PaletteProbe" Version="1.0.10" Language="en-US" Publisher="Rutile"/><DisplayName>Rutile isolated palette verification</DisplayName><Description>Test-only editor format probe.</Description></Metadata>
  <Installation><InstallationTarget Id="Microsoft.VisualStudio.Community" Version="[17.0,18.0)"><ProductArchitecture>amd64</ProductArchitecture></InstallationTarget></Installation>
  <Prerequisites><Prerequisite Id="Microsoft.VisualStudio.Component.CoreEditor" Version="[17.0,18.0)" DisplayName="Visual Studio core editor"/></Prerequisites>
  <Assets><Asset Type="Microsoft.VisualStudio.MefComponent" Path="Rutile.PaletteProbe.dll"/></Assets>
</PackageManifest>
'@ | Set-Content -LiteralPath $manifest
$files = Join-Path $output 'files.json'
@{ files = @($assembly | ForEach-Object { @{ culture=''; installRoot=''; ngen=$null; path=$_; targetPath=''; vsixSubPath='' } }) } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $files
$package = Join-Path $output 'Rutile.PaletteProbe.vsix'
$utility = Join-Path $installation 'VSSDK/VisualStudioIntegration/Tools/bin/VsixUtil.exe'
& $utility package -sourceManifest $manifest -outputPath $package -files $files -is64BitBuild | Out-Null
if ($LASTEXITCODE) {
    throw 'Palette probe packaging failed.'
}
$package
