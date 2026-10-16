param([string]$Rutile = "$PSScriptRoot/../../build/debug/bin/rutile.exe", [string]$Profile = 'debug')

$ErrorActionPreference = 'Stop'
$Rutile = (Resolve-Path -LiteralPath $Rutile).Path
$library = Get-Content -LiteralPath "$PSScriptRoot/../../compiler/src/std/unique.rt" -Raw
$workspace = [IO.Path]::GetFullPath("$PSScriptRoot/../../build/$Profile/tests/unique")
$runtime = @'
struct Tracker {
	fn Tracker(Tracker& this) :[default];
	fn Tracker(Tracker& this, i32* count);
	fn ~Tracker(Tracker& this);
	i32* count;
};
fn Tracker::Tracker(Tracker& this, i32* count) {
	@this.count = $count;
};
fn Tracker::~Tracker(Tracker& this) {
	@*this.count = *this.count + 1;
};
struct Deleter {
};
fn Deleter::dispose(Tracker* pointer) {
	@*pointer->count = *pointer->count + 10;
	delete pointer;
};
fn main() -> i32 {
	i32 count = 0;
	{
		unique<Tracker> empty;
		var null_owner = unique<Tracker>(nullptr);
		if (empty.get() || null_owner.get()) { return 1; };
		var first = unique<Tracker>(new Tracker(&count));
		var second = $first;
		var target = unique<Tracker>(new Tracker(&count));
		@target = $second;
		if (count != 1) { return 2; };
		var pointer = @target.release();
		if (target.get()) { return 3; };
		delete pointer;
		if (count != 2) { return 4; };
		@target.reset(new Tracker(&count));
		@target.reset(new Tracker(&count));
		if (count != 3) { return 5; };
	};
	if (count != 4) { return 6; };
	{
		var custom = unique<Tracker, Deleter>(new Tracker(&count));
	};
	if (count != 15) { return 7; };
	{
		var number = make_unique<i32>();
		if (*number.get() != 0) { return 8; };
		var custom = make_unique<Tracker, Deleter>();
		@custom.get()->count = &count;
	};
	if (count != 26) { return 9; };
	return 42;
};
'@
$checks = @(
	@{ name = 'runtime'; source = $runtime; exit = 42 },
	@{ name = 'copy'; source = 'fn main() -> i32 { var first = make_unique<i32>(); var second := first; return 42; };'; diagnostic = 'selected function is deleted' },
	@{ name = 'copy-assignment'; source = 'fn main() -> i32 { var first = make_unique<i32>(); unique<i32> second; @second := first; return 42; };'; diagnostic = 'selected function is deleted' }
)
foreach ($check in $checks) {
	$directory = Join-Path $workspace $check.name
	New-Item -ItemType Directory -Force "$directory/source" | Out-Null
	Set-Content -LiteralPath "$directory/source/main.rt" -Value ($library + $check.source) -Encoding utf8NoBOM
	Set-Content -LiteralPath "$directory/rutile.yaml" -Value @('targets:', '  app:', '    source: source', '    entry: main') -Encoding utf8NoBOM
	Push-Location $directory
	try {
		& $Rutile init | Out-Null
		if ($LASTEXITCODE) { throw 'Initialization failed.' }
		$output = & $Rutile run app --profile $Profile 2>&1 | Out-String
		$actual = $LASTEXITCODE
		if ($check.ContainsKey('exit')) {
			if ($actual -ne $check.exit) { throw "$($check.name): exit=$actual output=$output" }
		}
		elseif ($actual -ne 1 -or $output -notmatch $check.diagnostic) {
			throw "$($check.name): exit=$actual output=$output"
		}
		Write-Output "unique/$($check.name): PASS"
	}
	finally {
		& $Rutile exit 2>&1 | Out-Null
		Pop-Location
	}
}
