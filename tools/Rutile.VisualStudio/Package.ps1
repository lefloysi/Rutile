param(
	[ValidateSet('Debug', 'Release')][string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VSSDK -property installationPath
if (!$installation) {
	throw 'Visual Studio 2022 with the extension development workload is required.'
}
$expected_version = [IO.File]::ReadAllText((Join-Path $PSScriptRoot '../../version/release.txt')).Trim()
$msbuild = Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
Push-Location $PSScriptRoot
try {
	& $msbuild (Join-Path $PSScriptRoot 'Rutile.VisualStudio.csproj') /restore /p:NuGetAudit=false /p:RestoreIgnoreFailedSources=true '/t:Build;CreateVsixContainer' "/p:Configuration=$Configuration" '/p:DeployExtension=false'
}
finally {
	Pop-Location
}
if ($LASTEXITCODE) {
	throw "Rutile extension packaging failed with exit code $LASTEXITCODE."
}
$vsix = Join-Path $PSScriptRoot "bin/$Configuration/Rutile.VisualStudio.vsix"
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($vsix)
try {
	$reader = [IO.StreamReader]::new($archive.GetEntry('extension.vsixmanifest').Open())
	try {
		[xml]$manifest = $reader.ReadToEnd()
	}
	finally {
		$reader.Dispose()
	}
	if ($manifest.PackageManifest.Metadata.Identity.Id -ne 'Rutile.VisualStudio' -or $manifest.PackageManifest.Metadata.Identity.Version -ne $expected_version) {
		throw 'The Rutile VSIX identity or version does not match its source.'
	}
	if (!($manifest.PackageManifest.Dependencies.Dependency | Where-Object { $_.Id -eq 'Kiln.VisualStudio' })) {
		throw 'The Rutile VSIX must depend on the Kiln extension.'
	}
	$entry = $archive.GetEntry('Rutile.VisualStudio.dll')
	if (!$entry) {
		throw 'The Rutile VSIX is missing its language extension assembly.'
	}
	$hash = [Security.Cryptography.SHA256]::Create()
	$stream = $entry.Open()
	try {
		$packaged = [BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-', '')
		$source = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot "bin/$Configuration/net472/Rutile.VisualStudio.dll") -Algorithm SHA256).Hash
		if ($packaged -ne $source) {
			throw 'The packaged Rutile assembly does not match the built assembly.'
		}
	}
	finally {
		$stream.Dispose()
		$hash.Dispose()
	}
	foreach ($name in @('kiln.exe', 'LLVM-C.dll', 'Kiln.VisualStudio.dll', 'Kiln.VisualStudio.pkgdef', 'Rutile.VisualStudio.pkgdef')) {
		if ($archive.GetEntry($name)) {
			throw "The Rutile VSIX unexpectedly includes $name."
		}
	}
}
finally {
	$archive.Dispose()
}
if ($Configuration -eq 'Release') {
	$distributed = Join-Path $PSScriptRoot '../../distro/windows-x64/Rutile.VisualStudio.vsix'
	Copy-Item -LiteralPath $vsix -Destination $distributed -Force
}
Write-Output $vsix
