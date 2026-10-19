param([string]$Rutile = "$PSScriptRoot/../../build/debug/bin/rutile.exe", [string]$Profile = 'debug')

$ErrorActionPreference = 'Stop'
$Rutile = (Resolve-Path -LiteralPath $Rutile).Path
$library = Get-Content -LiteralPath "$PSScriptRoot/../../compiler/std/string.rt" -Raw
$workspace = [IO.Path]::GetFullPath("$PSScriptRoot/../../build/$Profile/tests/string")
$checks = @(
	@{ name = 'aliases'; fixture = 'string-aliases.rt' },
	@{ name = 'equality'; fixture = 'string-equality.rt' },
	@{ name = 'runtime'; fixture = 'string.rt' },
	@{ name = 'capacity'; fixture = 'string-capacity.rt' }
)
foreach ($check in $checks) {
	$directory = Join-Path $workspace $check.name
	New-Item -ItemType Directory -Force "$directory/source" | Out-Null
	$fixture = Get-Content -LiteralPath (Join-Path "$PSScriptRoot/fixtures" $check.fixture) -Raw
	Set-Content -LiteralPath "$directory/source/main.rt" -Value ($library + $fixture) -Encoding utf8NoBOM
	Set-Content -LiteralPath "$directory/rutile.yaml" -Value @('targets:', '  app:', '    source: source', '    entry: main') -Encoding utf8NoBOM
	Push-Location $directory
	try {
		$initialization = & $Rutile init 2>&1 | Out-String
		if ($LASTEXITCODE) {
			throw "Initialization failed: $initialization"
		}
		$output = & $Rutile run app --profile $Profile 2>&1 | Out-String
		if ($LASTEXITCODE -ne 42) {
			throw "$($check.name): exit=$LASTEXITCODE output=$output"
		}
		Write-Output "string/$($check.name): PASS"
	}
	finally {
		& $Rutile exit 2>&1 | Out-Null
		Pop-Location
	}
}
