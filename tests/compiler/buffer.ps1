param([string]$Rutile = "$PSScriptRoot/../../build/debug/bin/rutile.exe", [string]$Profile = 'debug')

$ErrorActionPreference = 'Stop'
$Rutile = (Resolve-Path -LiteralPath $Rutile).Path
$library = Get-Content -LiteralPath "$PSScriptRoot/../../kiln/compiler/std/buffer.rt" -Raw
$workspace = [IO.Path]::GetFullPath("$PSScriptRoot/../../build/$Profile/tests/buffer")
$checks = @(
	@{ name = 'numeric'; fixture = 'buffer.rt' },
	@{ name = 'lifetime'; fixture = 'buffer-lifetime.rt' },
	@{ name = 'alias'; fixture = 'buffer-alias.rt' },
	@{ name = 'const'; fixture = 'buffer-const.rt'; reject = $true }
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
		if ($check.reject) {
			if (!$LASTEXITCODE -or $output -notmatch 'cannot convert|incompatible|const|conversion') {
				throw "$($check.name): expected a const-access diagnostic, exit=$LASTEXITCODE output=$output"
			}
			Write-Output "buffer/$($check.name): PASS"
			continue
		}
		if ($LASTEXITCODE -ne 42) {
			throw "$($check.name): exit=$LASTEXITCODE output=$output"
		}
		Write-Output "buffer/$($check.name): PASS"
	}
	finally {
		& $Rutile exit 2>&1 | Out-Null
		Pop-Location
	}
}
