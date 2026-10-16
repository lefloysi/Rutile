param([string]$Compiler = "$PSScriptRoot/../../build/debug/bin/rutile-selfhost.exe")

$ErrorActionPreference = 'Stop'
$Compiler = (Resolve-Path -LiteralPath $Compiler).Path
$workspace = Join-Path $PSScriptRoot "../../build/tests/bootstrap-input/$([guid]::NewGuid())"
$workspace = [System.IO.Path]::GetFullPath($workspace)
$directory = Join-Path $workspace 'source'
New-Item -ItemType Directory -Force (Join-Path $directory 'nested') | Out-Null

function Compile-Input([string]$Source, [string]$Output, [int]$Expected = 0) {
	$message = & $Compiler $Source --output $Output --optimization 0 2>&1 | Out-String
	if ($LASTEXITCODE -ne $Expected) {
		throw "Unexpected compiler exit $LASTEXITCODE for '$Source': $message"
	}
	return $message
}

# Creation order differs from path order; extensionless and nested binary-named files count.
# The first boundary splits the token 'fn', so inserting a separator would break this input.
[System.IO.File]::WriteAllText((Join-Path $directory 'zz-end.bin'), ';', [System.Text.UTF8Encoding]::new($false))
[System.IO.File]::WriteAllText((Join-Path $directory 'nested/body.data'), 'n included()->i32 { return 42; }', [System.Text.UTF8Encoding]::new($false))
[System.IO.File]::WriteAllText((Join-Path $directory '00-start'), 'f', [System.Text.UTF8Encoding]::new($false))
$reference = Join-Path $workspace 'reference.rt'
[System.IO.File]::WriteAllText($reference, 'fn included()->i32 { return 42; };', [System.Text.UTF8Encoding]::new($false))
$combined = Join-Path $workspace 'combined.obj'
$ordinary = Join-Path $workspace 'ordinary.obj'
Compile-Input $directory $combined | Out-Null
Compile-Input $reference $ordinary | Out-Null
if ((Get-FileHash -LiteralPath $combined).Hash -ne (Get-FileHash -LiteralPath $ordinary).Hash) {
	throw 'Directory input differs from the exact concatenated file.'
}

$member = Join-Path $directory 'nested/body.data'
$original = (Get-FileHash -LiteralPath $member).Hash
$message = Compile-Input $directory $member 1
if ($message -notmatch 'overwrite' -or (Get-FileHash -LiteralPath $member).Hash -ne $original) {
	throw 'Directory member overwrite was not rejected without changing the source.'
}
$message = Compile-Input $reference $reference 1
if ($message -notmatch 'overwrite') {
	throw 'Ordinary source overwrite protection regressed.'
}

$empty = Join-Path $workspace 'empty'
New-Item -ItemType Directory $empty | Out-Null
$empty_file = Join-Path $workspace 'empty.rt'
[System.IO.File]::WriteAllBytes($empty_file, [byte[]]::new(0))
Compile-Input $empty (Join-Path $workspace 'empty-directory.obj') | Out-Null
Compile-Input $empty_file (Join-Path $workspace 'empty-file.obj') | Out-Null
if ((Get-FileHash -LiteralPath (Join-Path $workspace 'empty-directory.obj')).Hash -ne (Get-FileHash -LiteralPath (Join-Path $workspace 'empty-file.obj')).Hash) {
	throw 'Empty directory input differs from an empty source file.'
}
Write-Output 'Bootstrap directory input: PASS'
