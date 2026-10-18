param(
	[string]$Compiler = "$PSScriptRoot/../../../build/bootstrap/compiler.exe",
	[string]$DeveloperCommand = '',
	[string]$Output = "$PSScriptRoot/../../../build/tests/compiler-new/std"
)

$ErrorActionPreference = 'Stop'
$Compiler = (Resolve-Path -LiteralPath $Compiler).Path
$root = [IO.Path]::GetFullPath("$PSScriptRoot/../../..")
$Output = [IO.Path]::GetFullPath($Output)
if (!$DeveloperCommand) {
	$locator = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
	$installation = & $locator -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
	if ($LASTEXITCODE -or !$installation) {
		throw 'Visual Studio native build tools were not found.'
	}
	$DeveloperCommand = Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
}
$DeveloperCommand = (Resolve-Path -LiteralPath $DeveloperCommand).Path
$libraries = @(
	"$root/kiln/compiler/std/unique.rt",
	"$root/kiln/compiler/std/buffer.rt",
	"$root/kiln/compiler/std/string_view.rt",
	"$root/kiln/compiler/std/string.rt"
)
$checks = @(
	@{ name = 'buffer'; fixture = 'buffer.rt' },
	@{ name = 'buffer-lifetime'; fixture = 'buffer-lifetime.rt' },
	@{ name = 'buffer-alias'; fixture = 'buffer-alias.rt' },
	@{ name = 'buffer-owners'; fixture = 'buffer-owners.rt' },
	@{ name = 'unique'; fixture = 'unique.rt' },
	@{ name = 'text'; fixture = 'text.rt' },
	@{ name = 'buffer-const'; fixture = 'buffer-const.rt'; diagnostic = 'pointer conversion cannot discard const qualification' },
	@{ name = 'buffer-copy'; fixture = 'buffer-copy.rt'; diagnostic = 'deleted' },
	@{ name = 'unique-copy'; fixture = 'unique-copy.rt'; diagnostic = 'deleted' }
)
New-Item -ItemType Directory -Force $Output | Out-Null
foreach ($check in $checks) {
	$object = Join-Path $Output "$($check.name).obj"
	$executable = Join-Path $Output "$($check.name).exe"
	$fixture = Join-Path $PSScriptRoot $check.fixture
	$diagnostics = & $Compiler @libraries $fixture --output $object 2>&1 | Out-String
	$status = $LASTEXITCODE
	if ($check.diagnostic) {
		if ($status -ne 1 -or !$diagnostics.Contains($check.diagnostic)) {
			throw "$($check.name): expected '$($check.diagnostic)', exit=$status output=$diagnostics"
		}
		Write-Output "kiln/compiler/std/$($check.name): PASS"
		continue
	}
	if ($status) {
		throw "$($check.name): compilation failed, exit=$status output=$diagnostics"
	}
	$linkCommand = '"' + $DeveloperCommand + '" -no_logo -arch=x64 && link /nologo /subsystem:console /entry:mainCRTStartup "' + $object + '" libcmt.lib libvcruntime.lib libucrt.lib oldnames.lib kernel32.lib /out:"' + $executable + '"'
	$linkOutput = & cmd /d /s /c $linkCommand 2>&1 | Out-String
	if ($LASTEXITCODE) {
		throw "$($check.name): linking failed: $linkOutput"
	}
	& $executable
	if ($LASTEXITCODE -ne 42) {
		throw "$($check.name): expected exit 42, got $LASTEXITCODE"
	}
	Write-Output "kiln/compiler/std/$($check.name): PASS"
}
