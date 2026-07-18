[CmdletBinding()]
param(
	[string]$Configuration = "Debug",
	[string]$BuildDirectory = "out/build/x64-Debug",
	[ValidateRange(100, 60000)]
	[int]$RunMilliseconds = 1000,
	[ValidateRange(1, 60)]
	[int]$StartupTimeoutSeconds = 10,
	[ValidateRange(1, 60)]
	[int]$ShutdownTimeoutSeconds = 5
)

$ErrorActionPreference = "Stop"

$repository = Split-Path -Parent $PSScriptRoot
if (![IO.Path]::IsPathRooted($BuildDirectory)) {
	$BuildDirectory = Join-Path $repository $BuildDirectory
}
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)

$examples = @(
	"rutile-01-triangle",
	"rutile-02-spinning-cube",
	"rutile-03-plasma-lab",
	"rutile-04-galaxy",
	"rutile-05-voxel-renderer"
)
$backends = @("rt-vk13", "rt-dx12")

function Initialize-MsvcEnvironment {
	if ($env:INCLUDE) {
		return
	}

	$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
	if (!(Test-Path -LiteralPath $vswhere)) {
		throw "Could not find vswhere.exe. Install Visual Studio with the C++ desktop workload."
	}
	$vcvars = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find "VC/Auxiliary/Build/vcvars64.bat"
	if (!$vcvars) {
		throw "Could not find vcvars64.bat. Install Visual Studio with the C++ desktop workload."
	}

	$environment = & $env:COMSPEC /d /s /c "`"$vcvars`" >nul && set"
	if ($LASTEXITCODE -ne 0) {
		throw "Failed to initialize the Visual Studio C++ environment."
	}
	foreach ($entry in $environment) {
		if ($entry -match "^([^=]+)=(.*)$") {
			Set-Item -LiteralPath "Env:$($Matches[1])" -Value $Matches[2]
		}
	}
}

function Invoke-CMake {
	param([Parameter(ValueFromRemainingArguments)] [string[]]$Arguments)

	& cmake @Arguments
	if ($LASTEXITCODE -ne 0) {
		throw "cmake failed with exit code $LASTEXITCODE"
	}
}

function Find-ExampleBinaryDirectory {
	$candidates = @(
		(Join-Path $BuildDirectory "bin/$Configuration"),
		(Join-Path $BuildDirectory "bin")
	)
	foreach ($candidate in $candidates) {
		if (Test-Path -LiteralPath (Join-Path $candidate "$($examples[0]).exe")) {
			return $candidate
		}
	}
	throw "Could not find the built examples under '$BuildDirectory'."
}

function Run-Example {
	param(
		[string]$BinaryDirectory,
		[string]$Example,
		[string]$Backend
	)

	$startInfo = [Diagnostics.ProcessStartInfo]::new()
	$startInfo.FileName = Join-Path $BinaryDirectory "$Example.exe"
	$startInfo.Arguments = "--backend $Backend"
	$startInfo.WorkingDirectory = $BinaryDirectory
	$startInfo.UseShellExecute = $false
	$startInfo.RedirectStandardOutput = $true
	$startInfo.RedirectStandardError = $true

	$process = [Diagnostics.Process]::new()
	$process.StartInfo = $startInfo
	[void]$process.Start()
	$stdoutTask = $process.StandardOutput.ReadToEndAsync()
	$stderrTask = $process.StandardError.ReadToEndAsync()
	$failure = $null

	try {
		$startupDeadline = [DateTime]::UtcNow.AddSeconds($StartupTimeoutSeconds)
		while (!$process.HasExited -and $process.MainWindowHandle -eq [IntPtr]::Zero -and [DateTime]::UtcNow -lt $startupDeadline) {
			Start-Sleep -Milliseconds 50
			$process.Refresh()
		}

		if ($process.HasExited) {
			$failure = "exited before opening a window"
		} elseif ($process.MainWindowHandle -eq [IntPtr]::Zero) {
			$failure = "did not open a window within $StartupTimeoutSeconds seconds"
		} else {
			Start-Sleep -Milliseconds $RunMilliseconds
			if (!$process.HasExited -and !$process.CloseMainWindow()) {
				$failure = "did not accept a graceful close request"
			}
		}

		if (!$process.HasExited -and !$process.WaitForExit($ShutdownTimeoutSeconds * 1000)) {
			$failure = "did not exit within $ShutdownTimeoutSeconds seconds"
		}
	} finally {
		if (!$process.HasExited) {
			$process.Kill($true)
			$process.WaitForExit()
		}
	}

	$stdout = $stdoutTask.GetAwaiter().GetResult().Trim()
	$stderr = $stderrTask.GetAwaiter().GetResult().Trim()
	if (!$failure -and $process.ExitCode -ne 0) {
		$failure = "exited with code $($process.ExitCode)"
	}

	[pscustomobject]@{
		Example = $Example
		Backend = $Backend
		Result = if ($failure) { "FAIL" } else { "PASS" }
		Details = if ($failure) { $failure } else { "exit 0" }
		Stdout = $stdout
		Stderr = $stderr
	}
}

Initialize-MsvcEnvironment

Write-Host "Configuring Vulkan, DirectX 12, and examples..."
Invoke-CMake -S $repository -B $BuildDirectory "-DCMAKE_BUILD_TYPE=$Configuration" -DRUTILE_BUILD_EXAMPLES=ON -DRUTILE_BUILD_VK13=ON -DRUTILE_BUILD_DX12=ON

Write-Host "Building examples..."
Invoke-CMake --build $BuildDirectory --config $Configuration --parallel --target @examples

$binaryDirectory = Find-ExampleBinaryDirectory
$results = foreach ($backend in $backends) {
	foreach ($example in $examples) {
		Write-Host "Running $example with $backend..."
		Run-Example -BinaryDirectory $binaryDirectory -Example $example -Backend $backend
	}
}

$results | Format-Table Example, Backend, Result, Details -AutoSize
$failures = @($results | Where-Object Result -eq "FAIL")
if ($failures.Count -ne 0) {
	foreach ($failure in $failures) {
		Write-Error "$($failure.Example) [$($failure.Backend)]: $($failure.Details)`nstdout:`n$($failure.Stdout)`nstderr:`n$($failure.Stderr)" -ErrorAction Continue
	}
	exit 1
}

Write-Host "All $($results.Count) example/backend combinations passed."
