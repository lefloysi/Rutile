$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$output = Join-Path $root 'build/vsix-tests/template-arguments'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$installation = & "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VSSDK -property installationPath
$compiler = Join-Path $installation 'MSBuild/Current/Bin/Roslyn/csc.exe'
$desktop = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/WPF'
$assembly = Join-Path $PSScriptRoot '../bin/Debug/net472'
$references = @('PresentationCore.dll', 'PresentationFramework.dll') | ForEach-Object { '/reference:' + (Join-Path $desktop $_) }
$references += '/reference:' + (Join-Path $desktop 'WindowsBase.dll')
$references += '/reference:' + (Join-Path $assembly 'Newtonsoft.Json.dll')
$exe = Join-Path $output 'TemplateArgumentsTests.exe'
& $compiler /nologo /langversion:latest /nullable:enable /target:exe "/out:$exe" @references (Join-Path $PSScriptRoot '../TemplateContextLayout.cs') (Join-Path $PSScriptRoot 'TemplateArgumentsTests.cs')
if ($LASTEXITCODE) { throw 'Template argument tests compilation failed' }
Copy-Item (Join-Path $assembly 'Newtonsoft.Json.dll') $output -Force
& $exe
if ($LASTEXITCODE) { throw 'Template argument tests failed' }

$widget_exe = Join-Path $output 'TemplateWidgetTests.exe'
& $compiler /nologo /langversion:latest /nullable:enable /target:exe "/out:$widget_exe" /reference:System.Xaml.dll @references (Join-Path $PSScriptRoot '../TemplateArgumentWidget.cs') (Join-Path $PSScriptRoot 'TemplateWidgetTests.cs')
if ($LASTEXITCODE) {
	throw 'Template widget tests compilation failed'
}
& $widget_exe (Join-Path $output 'compact.png')
if ($LASTEXITCODE) {
	throw 'Template widget tests failed'
}
