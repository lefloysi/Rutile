param([string]$Rutile = "$PSScriptRoot/../../build/debug/bin/rutile.exe", [string]$Profile = "debug")

$ErrorActionPreference = 'Stop'
$Rutile = (Resolve-Path -LiteralPath $Rutile).Path
$cases = Join-Path $PSScriptRoot 'cases'
$directory = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "../../build/$Profile/tests/compiler/session"))
New-Item -ItemType Directory -Path $directory -Force | Out-Null
$targets = [ordered]@{
	templates = 'template-reuse'
	constants = 'constant-control-flow'
	construction = 'template-typed-construction'
	components = 'lifecycle-components'
	dispatch = 'virtual-dispatch'
	interfaces = 'call-interface-template-explicit'
	invalid = 'recursive-value'
	lifecycle = 'lifecycle-return'
}
$configuration = @('targets:')
foreach ($target in $targets.GetEnumerator()) {
	$source = (Resolve-Path -LiteralPath (Join-Path $cases $target.Value)).Path
	$configuration += @("  $($target.Key):", "    source: $($source | ConvertTo-Json -Compress)", '    entry: main')
}
Set-Content -LiteralPath (Join-Path $directory 'rutile.yaml') -Value $configuration -Encoding utf8NoBOM

Push-Location $directory
try {
	$initialization = & $Rutile init 2>&1 | Out-String
	if ($LASTEXITCODE) {
		throw "Initialization failed: $initialization"
	}
	foreach ($target in @('templates', 'constants', 'construction', 'components', 'dispatch', 'interfaces', 'invalid', 'lifecycle', 'components', 'constants', 'interfaces', 'construction', 'dispatch', 'templates')) {
		$output = & $Rutile run $target --profile $Profile 2>&1 | Out-String
		$actual = $LASTEXITCODE
		if ($target -eq 'invalid') {
			if ($actual -ne 1 -or $output -notmatch 'recursive|incomplete|cycle') {
				throw "Expected recursive-type rejection: exit=$actual output=$output"
			}
		}
		elseif ($actual -ne 42) {
			throw "Session compilation $target failed: exit=$actual output=$output"
		}
		Write-Output "session/$($target): PASS"
	}
}
finally {
	& $Rutile exit 2>&1 | Out-Null
	Pop-Location
}
