param([string]$Rutile = "$PSScriptRoot/../../build/debug/bin/rutile.exe", [string]$Profile = 'debug')

$ErrorActionPreference = 'Stop'
$Rutile = (Resolve-Path -LiteralPath $Rutile).Path
$library = Get-Content -LiteralPath "$PSScriptRoot/../../compiler/std/unique.rt" -Raw
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
	if (!pointer) {
		return;
	};
	@*pointer->count = *pointer->count + 10;
	delete pointer;
};
fn main() -> i32 {
	i32 count = 0;
	{
		rt::unique<Tracker> empty;
		var null_owner = rt::unique<Tracker>(nullptr);
		if (empty.get() || null_owner.get()) { return 1; };
		@empty.reset(nullptr);
		var first = rt::unique<Tracker>(new Tracker(&count));
		var second = $first;
		var target = rt::unique<Tracker>(new Tracker(&count));
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
		rt::unique<Tracker, Deleter> empty;
		@empty.reset(nullptr);
		var custom = rt::unique<Tracker, Deleter>(new Tracker(&count));
	};
	if (count != 15) { return 7; };
	{
		var number = rt::unique<i32>::make();
		if (*number.get() != 0) { return 8; };
		var custom = rt::unique<Tracker, Deleter>::make();
		@custom.get()->count = &count;
	};
	if (count != 26) { return 9; };
	return 42;
};
'@
$checks = @(
	@{ name = 'namespace-type'; source = 'struct Box {}; namespace Box { fn injected() {} }; fn main() -> i32 { return 42; };'; diagnostic = 'namespace' },
	@{ name = 'namespace-union'; source = 'union Box { i32 value; }; namespace Box { fn injected() {} }; fn main() -> i32 { return 42; };'; diagnostic = 'namespace' },
	@{ name = 'namespace-enum'; source = 'enum Box { value; }; namespace Box { fn injected() {} }; fn main() -> i32 { return 42; };'; diagnostic = 'namespace' },
	@{ name = 'namespace-template-type'; source = 'template<typename T> struct Box {}; namespace Box { fn injected() {} }; fn main() -> i32 { return 42; };'; diagnostic = 'namespace' },
	@{ name = 'namespace-specialization'; source = 'template<typename T> struct Box {}; namespace Box<i32> { fn injected() {} }; fn main() -> i32 { return 42; };'; diagnostic = 'namespace' },
	@{ name = 'namespace-arguments'; source = 'namespace sample {}; namespace sample<i32> { fn injected() {} }; fn main() -> i32 { return 42; };'; diagnostic = 'namespace|interpretation' },
	@{ name = 'namespace-type-child'; source = 'struct Box {}; namespace Box::child { fn injected() {} }; fn main() -> i32 { return 42; };'; diagnostic = 'namespace' },
	@{ name = 'namespace-specialization-child'; source = 'template<typename T> struct Box {}; namespace Box<i32>::child { fn injected() {} }; fn main() -> i32 { return 42; };'; diagnostic = 'namespace' },
	@{ name = 'namespace-before-type'; source = 'namespace Box {}; struct Box {}; fn main() -> i32 { return 42; };'; diagnostic = 'conflict|namespace|declaration' },
	@{ name = 'namespace-template'; source = 'template<typename T> namespace Box {}; fn main() -> i32 { return 42; };'; exit = 42 },
	@{ name = 'namespace-reopen'; source = 'namespace sample { fn first() -> i32 { return 20; }; }; namespace sample { fn second() -> i32 { return 22; }; }; fn main() -> i32 { return sample::first() + sample::second(); };'; exit = 42 },
	@{ name = 'constructor-arguments'; source = (Get-Content -LiteralPath "$PSScriptRoot/cases/template-constructor-arguments/main.rt" -Raw); exit = 42 },
	@{ name = 'runtime'; source = $runtime; exit = 42 },
	@{ name = 'copy'; source = 'fn main() -> i32 { var first = rt::unique<i32>::make(); var second := first; return 42; };'; diagnostic = 'selected function is deleted' },
	@{ name = 'copy-assignment'; source = 'fn main() -> i32 { var first = rt::unique<i32>::make(); rt::unique<i32> second; @second := first; return 42; };'; diagnostic = 'selected function is deleted' }
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
