param(
    [Parameter(Mandatory = $true)] [string]$Vsix,
    [string]$RootSuffix = 'RutileValidation7701',
    [switch]$Install
)

$ErrorActionPreference = 'Stop'
if ([Threading.Thread]::CurrentThread.GetApartmentState() -ne 'STA') {
    throw 'Run this verification with powershell.exe -STA.'
}
if ($RootSuffix -notmatch '^RutileValidation[0-9]+$') {
    throw 'Use a dedicated RutileValidation<number> profile.'
}
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VSSDK -format json | ConvertFrom-Json | Select-Object -First 1
$existing = @(Get-CimInstance Win32_Process -Filter "Name='devenv.exe'" | Where-Object { $_.CommandLine -match [regex]::Escape($RootSuffix) })
if ($existing.Count) {
    throw 'The dedicated profile is already running; it will not be disturbed.'
}
$output = Join-Path $root ('build/vsix-tests/' + $RootSuffix)
$project = Join-Path $output ('workspace-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Force -Path (Join-Path $project 'native'), (Join-Path $project 'rutile') | Out-Null
[IO.File]::WriteAllText((Join-Path $project 'native/main.cpp'), "#include <windows.h>`nint main() {`n    Sleep(2000);`n    return 42;`n}`n")
[IO.File]::WriteAllText((Join-Path $project 'rutile/main.rt'), 'fn main() -> i32 { return 42; };')
[IO.File]::WriteAllText((Join-Path $project 'kiln.rt'), @'
fn main(kiln::Options& options) {
    var native = kiln::add_target("native.exe", kiln::TargetType::executable);
    @native.compiler("cpp");
    @native.sources.add("native");
    @native.option("standard", "c++20");
    @native.option("libraries", "kernel32.lib");
    var language = kiln::add_target("rt-app.exe", kiln::TargetType::executable);
    @language.sources.add("rutile");
};
'@)

if ($Install) {
    $start = [Diagnostics.ProcessStartInfo]::new((Join-Path $installation.installationPath 'Common7/IDE/VSIXInstaller.exe'))
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $start.Arguments = '/quiet /rootSuffix:' + $RootSuffix + ' /instanceids:' + $installation.instanceId + ' "' + [IO.Path]::GetFullPath($Vsix) + '"'
    $deployment = [Diagnostics.Process]::Start($start)
    if (!$deployment.WaitForExit(180000)) {
        & taskkill.exe /PID $deployment.Id /T /F | Out-Null
        throw 'The isolated extension installer timed out.'
    }
    if ($deployment.ExitCode) {
        throw ('Installing into the dedicated profile failed with exit code ' + $deployment.ExitCode + '.')
    }
    $deployment.Dispose()
}

$interop = Join-Path $installation.installationPath 'Common7/IDE/PublicAssemblies/Microsoft.VisualStudio.Interop.dll'
[Reflection.Assembly]::LoadFrom($interop) | Out-Null
Add-Type -Path (Join-Path $PSScriptRoot 'VisualStudioAutomation.cs') -ReferencedAssemblies $interop
[VisualStudioAutomation]::SuppressCrashDialogs()
$filter = [VisualStudioMessageFilter]::new()
$process = $null
$watchdog = $null
$script:dte = $null
$results = [Collections.Generic.List[string]]::new()

function Wait-For([scriptblock]$Predicate, [string]$Description) {
    $timeout = [Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        if (& $Predicate) {
            return
        }
        if ($timeout.Elapsed.TotalSeconds -gt 90) {
            throw ('Timed out: ' + $Description)
        }
        Start-Sleep -Milliseconds 200
    }
}

try {
    $start = [Diagnostics.ProcessStartInfo]::new((Join-Path $installation.installationPath 'Common7/IDE/devenv.exe'))
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $start.Arguments = '"' + $project + '" /RootSuffix ' + $RootSuffix + ' /Log "' + (Join-Path $output 'ActivityLog.xml') + '"'
    [IO.File]::WriteAllText((Join-Path $output 'launch.txt'), $start.FileName + ' ' + $start.Arguments)
    $process = [Diagnostics.Process]::Start($start)
    $watchdog = [OwnedProcessTimeout]::new($process, 240000)
    Wait-For { $script:dte = [VisualStudioAutomation]::Find($process.Id, $RootSuffix); $script:dte -ne $null } 'dedicated automation object'
    $script:dte.SuppressUI = $true
    $script:dte.MainWindow.Visible = $false
    Write-Output ('Attached dedicated Visual Studio: ' + $script:dte.RegistryRoot)
    Write-Output ('Opening workspace: ' + $project)
    $script:dte.ExecuteCommand('File.OpenFolder', '"' + $project + '"')
    $script:dte.ItemOperations.OpenFile((Join-Path $project 'native/main.cpp')) | Out-Null
    try {
        Write-Output ('Kiln menu registered: ' + $script:dte.Commands.Item('Tools.Kiln').Name)
    }
    catch [Runtime.InteropServices.COMException] {
        Write-Output ('Kiln menu lookup: ' + $_.Exception.Message)
    }
    Wait-For { Test-Path -LiteralPath (Join-Path $project '.vs/tasks.vs.json') } 'Kiln workspace auto-load'
    $tasks = [IO.File]::ReadAllText((Join-Path $project '.vs/tasks.vs.json')) | ConvertFrom-Json
    if ($tasks.tasks.Count -ne 2 -or !(Test-Path -LiteralPath (Join-Path $project 'CppProperties.json'))) {
        throw 'The open folder did not receive native and Rutile tasks plus C++ IntelliSense settings.'
    }
    $results.Add('open-folder auto-load, target discovery, C++ IntelliSense configuration')
    $script:dte.ExecuteCommand('Build.BuildSolution')
    $artifact = $tasks.tasks[0].output
    Wait-For { Test-Path -LiteralPath $artifact } 'native Build command'
    $results.Add('native Visual Studio Build command')

    $languageBefore = @(Get-CimInstance Win32_Process -Filter "Name='kiln.exe'" | Where-Object { $_.CommandLine -match 'language-server' } | ForEach-Object ProcessId)
    $script:dte.ItemOperations.OpenFile((Join-Path $project 'rutile/main.rt')) | Out-Null
    Wait-For {
        @(Get-CimInstance Win32_Process -Filter "Name='kiln.exe'" | Where-Object { $_.CommandLine -match 'language-server' -and $_.ProcessId -notin $languageBefore }).Count -gt 0
    } 'Rutile language connection alongside persistent project session'
    $results.Add('Rutile language service alongside generic project session')

    $script:dte.ItemOperations.OpenFile((Join-Path $project 'native/main.cpp')) | Out-Null
    $script:dte.Debugger.Breakpoints.Add('', (Join-Path $project 'native/main.cpp'), 3) | Out-Null
    $script:dte.ExecuteCommand('Debug.Start')
    Wait-For { [int]$script:dte.Debugger.CurrentMode -eq 2 } 'native F5 breakpoint'
    $results.Add('native F5 debugger and source breakpoint')
    $script:dte.ExecuteCommand('Debug.StopDebugging')

    $nativeSource = Join-Path $project 'native/main.cpp'
    $savedSource = [IO.File]::ReadAllText($nativeSource)
    [IO.File]::WriteAllText($nativeSource, 'int main() { missing_symbol; }')
    Wait-For { $script:dte.ToolWindows.ErrorList.ErrorItems.Count -gt 0 } 'background rebuild diagnostics in Error List'
    [IO.File]::WriteAllText($nativeSource, $savedSource)
    Wait-For { $script:dte.ToolWindows.ErrorList.ErrorItems.Count -eq 0 } 'background rebuild recovery'
    $results.Add('continuous source rebuild failure and recovery in Error List')
}
finally {
    if ($script:dte) {
        try {
            $script:dte.Quit()
        }
        catch [Runtime.InteropServices.COMException] {
            Write-Warning $_.Exception.Message
        }
    }
    if ($process -and !$process.HasExited -and !$process.WaitForExit(30000)) {
        $process.Kill()
    }
    if ($watchdog) {
        $watchdog.Dispose()
    }
    $filter.Dispose()
    @{ profile = $RootSuffix; completed = $results.ToArray(); workspace = $project } | ConvertTo-Json | Set-Content (Join-Path $output 'kiln-results.json')
}
