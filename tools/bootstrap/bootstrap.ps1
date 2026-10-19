[CmdletBinding()]
param(
	[string]$Dependencies,
	[string]$Llvm,
	[ValidateSet('debug', 'release')][string]$Profile = 'debug',
	[switch]$PrepareOnly,
	[switch]$ReuseLanguageObject
)
$ErrorActionPreference = 'Stop'
$project_root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$bootstrap = Join-Path $project_root '../Kiln/tools/bootstrap/bootstrap.ps1'
if (!(Test-Path -LiteralPath $bootstrap)) {
	throw "Kiln bootstrap was not found: $bootstrap. Keep Kiln beside the Rutile folder."
}
& $bootstrap @PSBoundParameters -Rutile $project_root
