param([string]$Rutile = "$PSScriptRoot/../../build/debug/bin/rutile.exe", [string]$Profile = "debug", [string]$CasePattern = '*')

$ErrorActionPreference = 'Stop'
$Rutile = (Resolve-Path -LiteralPath $Rutile).Path
$cases = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'cases.json') -Raw | ConvertFrom-Json
$cases = @($cases | Where-Object { $_.name -like $CasePattern })
if (!$cases.Count) {
	throw "No compiler cases match '$CasePattern'."
}
$sources = Join-Path $PSScriptRoot 'cases'
$outputs = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "../../build/$Profile/tests/compiler"))
$failures = @()
foreach ($case in $cases) {
	if (!$case.name -or ($null -eq $case.exit -and !$case.diagnostic)) {
		throw 'A compiler case requires a name and an expected exit code or diagnostic.'
	}
	$source = (Resolve-Path -LiteralPath (Join-Path $sources $case.name)).Path
	$directory = Join-Path $outputs $case.name
	New-Item -ItemType Directory -Path $directory -Force | Out-Null
	$configuration = @('targets:', '  app:', "    source: $($source | ConvertTo-Json -Compress)", '    entry: main')
	Set-Content -LiteralPath (Join-Path $directory 'rutile.yaml') -Value $configuration -Encoding utf8NoBOM
	Push-Location $directory
	try {
		$initialization = & $Rutile init 2>&1 | Out-String
		if ($LASTEXITCODE) {
			throw "Initialization failed: $initialization"
		}
		$output = & $Rutile run app --profile $Profile 2>&1 | Out-String
		$actual = $LASTEXITCODE
		$passed = if ($null -ne $case.exit) { $actual -eq $case.exit } else { $actual -eq 1 -and $output -match $case.diagnostic }
		if (!$passed) {
			$failures += "$($case.name): exit=$actual output=$output"
		}
		Write-Output "$($case.name): $(if ($passed) { 'PASS' } else { 'FAIL' })"
	}
	finally {
		& $Rutile exit 2>&1 | Out-Null
		Pop-Location
	}
}
if ($failures.Count) {
	$failures | ForEach-Object { Write-Output $_ }
	exit 1
}

