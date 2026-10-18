param(
	[ValidateSet('Debug', 'Release')] [string]$Configuration = 'Release',
	[string]$RutileServerDirectory,
	[string]$RutileCrtDirectory,
	[string]$RutileDebugCrtDirectory,
	[string]$RutileUcrtDirectory
)

$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VSSDK -property installationPath
if (!$installation) {
	throw 'Visual Studio 2022 with the Visual Studio extension development workload is required.'
}

if (!$RutileServerDirectory) {
	$RutileServerDirectory = Join-Path $PSScriptRoot ("../../build/" + $Configuration.ToLowerInvariant() + '/bin')
}
$RutileServerDirectory = [IO.Path]::GetFullPath($RutileServerDirectory)
$kiln = Join-Path $RutileServerDirectory 'kiln.exe'
if (!(Test-Path -LiteralPath $kiln)) {
	throw "Build kiln first: $kiln is missing."
}
$expectedVersion = [IO.File]::ReadAllText((Join-Path $PSScriptRoot '../../version/release.txt')).Trim()
$nativeVersion = (& $kiln --version | Out-String).Trim()
if ($LASTEXITCODE -or $nativeVersion -cne $expectedVersion) {
	throw "Kiln version '$nativeVersion' does not match release '$expectedVersion'. Rebuild kiln before packaging."
}

$yaml = if ($Configuration -eq 'Debug') { 'yaml-cppd.dll' } else { 'yaml-cpp.dll' }
foreach ($name in @('kiln.exe', 'LLVM-C.dll', $yaml)) {
	if (!(Test-Path -LiteralPath (Join-Path $RutileServerDirectory $name))) {
		throw "Build kiln and its runtime first: $RutileServerDirectory/$name is missing."
	}
}

$runtime = Get-ChildItem (Join-Path $installation 'VC/Redist/MSVC') -Directory |
	Where-Object { $_.Name -match '^\d+\.\d+\.\d+$' } |
	Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
if (!$runtime) {
	throw 'The Visual Studio x64 MSVC redistributable directory was not found.'
}
if (!$RutileCrtDirectory) {
	$RutileCrtDirectory = Join-Path $runtime.FullName 'x64/Microsoft.VC143.CRT'
}
$RutileCrtDirectory = [IO.Path]::GetFullPath($RutileCrtDirectory)

$extra = @()
if ($Configuration -eq 'Debug') {
	if (!$RutileDebugCrtDirectory) {
		$RutileDebugCrtDirectory = Join-Path $runtime.FullName 'debug_nonredist/x64/Microsoft.VC143.DebugCRT'
	}
	if (!$RutileUcrtDirectory) {
		$sdk = Get-ChildItem (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits/10/bin') -Directory |
			Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
			Sort-Object { [version]$_.Name } -Descending |
			Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'x64/ucrt/ucrtbased.dll') } |
			Select-Object -First 1
		if (!$sdk) {
			throw 'The x64 Debug Universal CRT was not found in the installed Windows SDK.'
		}
		$RutileUcrtDirectory = Join-Path $sdk.FullName 'x64/ucrt'
	}
	$extra = @("/p:RutileDebugCrtDirectory=$([IO.Path]::GetFullPath($RutileDebugCrtDirectory))", "/p:RutileUcrtDirectory=$([IO.Path]::GetFullPath($RutileUcrtDirectory))")
}

$msbuild = Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
& $msbuild (Join-Path $PSScriptRoot 'Rutile.VisualStudio.csproj') /restore '/t:Build;CreateVsixContainer' "/p:Configuration=$Configuration" '/p:DeployExtension=false' "/p:RutileServerDirectory=$RutileServerDirectory" "/p:RutileCrtDirectory=$RutileCrtDirectory" @extra
if ($LASTEXITCODE) {
	exit $LASTEXITCODE
}

$vsix = Join-Path $PSScriptRoot "bin/$Configuration/Rutile.VisualStudio.vsix"
if (!(Test-Path -LiteralPath $vsix)) {
	throw "The Visual Studio SDK did not produce $vsix."
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($vsix)
try {
	$manifestEntry = $archive.GetEntry('extension.vsixmanifest')
	if (!$manifestEntry) {
		throw 'The VSIX is missing its manifest.'
	}
	$reader = [IO.StreamReader]::new($manifestEntry.Open())
	try {
		[xml]$manifest = $reader.ReadToEnd()
	}
	finally {
		$reader.Dispose()
	}
	if ($manifest.PackageManifest.Metadata.Identity.Version -ne $expectedVersion) {
		throw "Packaged manifest version does not match release '$expectedVersion'."
	}
	$payload = @('kiln.exe', 'LLVM-C.dll', $yaml)
	foreach ($name in $payload) {
		$entry = $archive.GetEntry($name)
		if (!$entry) {
			throw "The VSIX is missing $name."
		}
		$stream = $entry.Open()
		$hash = [Security.Cryptography.SHA256]::Create()
		try {
			$packagedHash = [BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-', '')
			$sourceHash = (Get-FileHash -LiteralPath (Join-Path $RutileServerDirectory $name) -Algorithm SHA256).Hash
			if ($packagedHash -ne $sourceHash) {
				throw "The packaged $name does not match the selected server payload."
			}
		}
		finally {
			$hash.Dispose()
			$stream.Dispose()
		}
	}
	$required = @('Rutile.VisualStudio.dll', 'vcruntime140.dll', 'msvcp140.dll')
	if ($Configuration -eq 'Debug') {
		$required += @('vcruntime140d.dll', 'msvcp140d.dll', 'ucrtbased.dll')
	}
	foreach ($name in $required) {
		if (!$archive.GetEntry($name)) {
			throw "The VSIX is missing $name."
		}
	}
}
finally {
	$archive.Dispose()
}
Write-Output $vsix
