$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$output = Join-Path $root 'build/vsix-tests/classifier-lifetime'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VSSDK -property installationPath
$msbuild = Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
& $msbuild (Join-Path $PSScriptRoot '../Rutile.VisualStudio.csproj') /t:Build /p:Configuration=Release /p:CreateVsixContainer=false /p:DeployExtension=false /p:NuGetAudit=false /nologo /v:minimal *> (Join-Path $output 'build.log')
if ($LASTEXITCODE) {
	throw ('Extension SDK compilation failed: ' + (Join-Path $output 'build.log'))
}

$compiler = Join-Path $installation 'MSBuild/Current/Bin/Roslyn/csc.exe'

$assembly = Join-Path $PSScriptRoot '../bin/Release/net472'
$references = Get-ChildItem $assembly -Filter '*.dll' | ForEach-Object { '/reference:' + $_.FullName }
$kilnAssembly = Join-Path $PSScriptRoot '../../../../Kiln/tools/Kiln.VisualStudio/bin/Release/net472/Kiln.VisualStudio.dll'
$references += '/reference:' + $kilnAssembly
Copy-Item -LiteralPath $kilnAssembly -Destination $output -Force
$references += '/reference:C:/Program Files (x86)/Reference Assemblies/Microsoft/Framework/.NETFramework/v4.7.2/Facades/netstandard.dll'
$exe = Join-Path $output 'ClassifierLifetimeTests.exe'
& $compiler /nologo /langversion:latest /target:exe "/out:$exe" @references (Join-Path $PSScriptRoot 'ClassifierLifetimeTests.cs')
if ($LASTEXITCODE) {
	throw 'Lifetime test compilation failed'
}

Get-ChildItem $assembly -Filter '*.dll' | Copy-Item -Destination $output -Force
$configuration = [xml]'<configuration><runtime><assemblyBinding xmlns="urn:schemas-microsoft-com:asm.v1" /></runtime></configuration>'
$binding = $configuration.configuration.runtime.FirstChild
Get-ChildItem $assembly -Filter '*.dll' | ForEach-Object {
	$name = [Reflection.AssemblyName]::GetAssemblyName($_.FullName)
	$token = -join ($name.GetPublicKeyToken() | ForEach-Object { $_.ToString('x2') })
	$dependency = $configuration.CreateElement('dependentAssembly', $binding.NamespaceURI)
	$identity = $configuration.CreateElement('assemblyIdentity', $binding.NamespaceURI)
	$identity.SetAttribute('name', $name.Name)
	$identity.SetAttribute('publicKeyToken', $token)
	$identity.SetAttribute('culture', 'neutral')
	$redirect = $configuration.CreateElement('bindingRedirect', $binding.NamespaceURI)
	$redirect.SetAttribute('oldVersion', '0.0.0.0-65535.65535.65535.65535')
	$redirect.SetAttribute('newVersion', $name.Version.ToString())
	[void]$dependency.AppendChild($identity)
	[void]$dependency.AppendChild($redirect)
	[void]$binding.AppendChild($dependency)
}

$configuration.Save($exe + '.config')
foreach ($order in @('attached-first', 'initialized-first')) {
	& $exe (Join-Path $output 'Rutile.VisualStudio.dll') $order
	if ($LASTEXITCODE) {
		throw ('Classifier RPC regression failed: ' + $order)
	}
}
