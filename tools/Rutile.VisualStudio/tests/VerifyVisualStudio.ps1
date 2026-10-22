param(
    [Parameter(Mandatory = $true)] [string]$Vsix,
    [switch]$Install,
    [string]$KilnVsix,
    [switch]$Palette,
    [switch]$PaletteOnly,
    [switch]$Typing,
    [switch]$DelayedCompiler,
    [switch]$FullShell,
    [switch]$Standalone,
    [string]$RootSuffix,
    [string]$Kiln
)

$ErrorActionPreference = 'Stop'
if ([Threading.Thread]::CurrentThread.GetApartmentState() -ne 'STA') {
    throw 'Run this verification with powershell.exe -STA.'
}
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$version = [IO.File]::ReadAllText((Join-Path $root 'VERSION')).Trim()
if (!$RootSuffix) {
    $RootSuffix = 'RutileValidation' + ($version -split '\.')[-1]
}
if ($RootSuffix -notmatch '^RutileValidation[0-9]+$') {
    throw 'Use a dedicated RutileValidation<number> profile; normal and Exp profiles are forbidden.'
}
$Vsix = [IO.Path]::GetFullPath($Vsix)
if (!$Kiln) {
    $Kiln = Join-Path $env:LOCALAPPDATA 'Programs/Kiln/bin/kiln.exe'
    if (!(Test-Path -LiteralPath $Kiln)) {
        $Kiln = Join-Path $root '../Kiln/distro/windows-x64/kiln.exe'
    }
}
$Kiln = [IO.Path]::GetFullPath($Kiln)
$existing = @(Get-CimInstance Win32_Process -Filter "Name='devenv.exe'" | Where-Object { $_.CommandLine -match [regex]::Escape($RootSuffix) })
if ($existing.Count) {
    throw 'The dedicated verification profile is already running; it will not be disturbed.'
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($Vsix)
try {
    $reader = [IO.StreamReader]::new($archive.GetEntry('extension.vsixmanifest').Open())
    try {
        [xml]$manifest = $reader.ReadToEnd()
    }
    finally {
        $reader.Dispose()
    }
    if ($manifest.PackageManifest.Metadata.Identity.Version -ne $version) {
        throw 'The VSIX does not match VERSION.'
    }
}
finally {
    $archive.Dispose()
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VSSDK -format json | ConvertFrom-Json | Select-Object -First 1
if (!$installation) {
    throw 'The installed Visual Studio SDK was not found.'
}
$output = Join-Path $root ('build/vsix-tests/' + $RootSuffix)
New-Item -ItemType Directory -Force -Path $output | Out-Null
if (!$KilnVsix) {
    $KilnVsix = Join-Path $PSScriptRoot '../../../../Kiln/tools/Kiln.VisualStudio/bin/Release/Kiln.VisualStudio.vsix'
}
$packages = @($Vsix)
if (!$Standalone) {
    $packages = @([IO.Path]::GetFullPath($KilnVsix)) + $packages
}
if ($Palette) {
    $probe = & (Join-Path $PSScriptRoot 'BuildPaletteProbe.ps1') -OutputDirectory (Join-Path $output 'probe')
    $packages += $probe
}
if ($Install) {
    foreach ($package in $packages) {
        $installer = [Diagnostics.ProcessStartInfo]::new((Join-Path $installation.installationPath 'Common7/IDE/VSIXInstaller.exe'))
        $installer.UseShellExecute = $false
        $installer.WorkingDirectory = $output
        $installer.CreateNoWindow = $true
        $installer.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
        $installer.Arguments = '/quiet /rootSuffix:' + $RootSuffix + ' /instanceids:' + $installation.instanceId + ' "' + $package + '"'
        $deployment = [Diagnostics.Process]::Start($installer)
        @{ pid = $deployment.Id; profile = $RootSuffix; executable = $installer.FileName; arguments = $installer.Arguments } |
            ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output ("installer-" + $deployment.Id + ".json"))
        if (!$deployment.WaitForExit(120000) -or $deployment.ExitCode) {
            throw 'Installing the VSIX into the dedicated profile failed.'
        }
        $deployment.Dispose()
    }
}

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

function New-Project([string]$Name) {
    $directory = Join-Path $output $Name
    New-Item -ItemType Directory -Force -Path (Join-Path $directory 'app') | Out-Null
    [IO.File]::WriteAllText((Join-Path $directory 'app/main.rt'), 'fn main()->i32 {return answer(42);};')
    [IO.File]::WriteAllText((Join-Path $directory 'app/library.rt'), 'fn answer(i32 value)->i32 {return value;};')
    if ($Palette) {
        [IO.File]::WriteAllText((Join-Path $directory 'app/main.rt'), @"
namespace sample {
struct Item {
    i32 field;
    fn read(Item& this)->i32 { return this.field; };
};
enum mode { executable; library; };
fn value()->i32 { Item thing; mode chosen=mode::executable; return thing.read(); };
}
fn main()->i32 { return sample::value(); };
fn typing_probe()->i32 {
    i32 completion_local = 42;
    // typing cursor
$("    ")
    return completion_local;
};
"@)
    }
    [IO.File]::WriteAllText((Join-Path $directory 'kiln.rt'), 'fn main(kiln::Options& options) { var target = kiln::add_target("app.exe", kiln::TargetType::executable); @target.sources.add("app"); };')
    [IO.File]::WriteAllText((Join-Path $directory 'Rutile.sln'), @'
Microsoft Visual Studio Solution File, Format Version 12.00
# Visual Studio Version 17
Project("{2150E333-8FDC-42A3-9474-1A3956D46DE8}") = "Sources", "Sources", "{7EFDBB69-D15E-4901-9021-026FB246BC30}"
    ProjectSection(SolutionItems) = preProject
        app\main.rt = app\main.rt
        app\library.rt = app\library.rt
        kiln.rt = kiln.rt
    EndProjectSection
EndProject
Global
EndGlobal
'@)
    return $directory
}

function Managed([string]$Directory) {
    Push-Location $Directory
    try {
        $status = (& $Kiln status 2>&1 | Out-String)
        return $status -match '(?m)^Kiln online\s*$' -and $status -match 'managed by the IDE'
    }
    finally {
        Pop-Location
    }
}

$interop = Join-Path $installation.installationPath 'Common7/IDE/PublicAssemblies/Microsoft.VisualStudio.Interop.dll'
[Reflection.Assembly]::LoadFrom($interop) | Out-Null
Add-Type -Path (Join-Path $PSScriptRoot 'VisualStudioAutomation.cs') -ReferencedAssemblies $interop
[VisualStudioAutomation]::SuppressCrashDialogs()
$filter = [VisualStudioMessageFilter]::new()
$first = New-Project 'first'
$second = New-Project 'second'
$process = $null
$compilerStartup = $null
$script:dte = $null
$results = [Collections.Generic.List[string]]::new()
try {
    $launch = [Diagnostics.ProcessStartInfo]::new((Join-Path $installation.installationPath 'Common7/IDE/devenv.exe'))
    $launch.UseShellExecute = $false
    $launch.CreateNoWindow = $true
    $launch.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $launch.WorkingDirectory = $output
    $launch.Arguments = '/RootSuffix ' + $RootSuffix + ' /Log "' + (Join-Path $output 'ActivityLog.xml') + '"'
    if (!$FullShell) {
        $launch.Arguments += ' /Embedding'
    }
    if ($Palette) {
        $paletteResult = Join-Path $output 'palette.txt'
        foreach ($resultFile in @($paletteResult,($paletteResult + '.ready'),($paletteResult + '.brushes'))) {
            if (Test-Path -LiteralPath $resultFile) {
                Remove-Item -LiteralPath $resultFile
            }
        }
        $launch.EnvironmentVariables['RUTILE_PALETTE_RESULT'] = $paletteResult
        if ($Typing) { $launch.EnvironmentVariables['RUTILE_TYPING_TEST'] = '1' }
        $launch.EnvironmentVariables['RUTILE_PALETTE_SOURCE'] = Join-Path $first 'app/main.rt'
    }
    $process = [Diagnostics.Process]::Start($launch)
    @{ pid = $process.Id; profile = $RootSuffix; executable = $launch.FileName; arguments = $launch.Arguments } |
        ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'owned-process.json')
    $watchdog = [OwnedProcessTimeout]::new($process, 180000)
    Wait-For { $script:dte = [VisualStudioAutomation]::Find($process.Id); $script:dte -ne $null } 'dedicated Visual Studio automation'
    Write-Output 'Dedicated automation object: attached'
    $script:dte.SuppressUI = $true
    try {
        $script:dte.MainWindow.Visible = $false
    }
    catch [Runtime.InteropServices.COMException] {
        Write-Warning ('Dedicated VS window visibility: ' + $_.Exception.Message)
    }
    Write-Output 'Opening dedicated test solution'
    $script:dte.Solution.Open((Join-Path $first 'Rutile.sln'))
    if ($PaletteOnly) {
        if ($DelayedCompiler) {
            $compilerStartup = Start-Job -ScriptBlock {
                param($directory, $executable)
                Start-Sleep -Seconds 35
                Set-Location -LiteralPath $directory
                & $executable configure --profile debug
                if ($LASTEXITCODE) { throw 'Delayed compiler configuration failed.' }
            } -ArgumentList $first, $Kiln
        }
        else {
            Push-Location $first
            try {
                & $Kiln configure --profile debug
                if ($LASTEXITCODE) { throw 'Palette compiler configuration failed.' }
            }
            finally { Pop-Location }
        }
    }
    $main = Join-Path $first 'app/main.rt'
    $script:dte.ItemOperations.OpenFile($main) | Out-Null
    if (!$PaletteOnly) {
        Wait-For { Managed $first } 'IDE-owned Kiln startup'
        $results.Add('IDE-owned project process')
        Write-Output 'IDE-owned project process: PASS'
    }
    if ($Palette) {
        $probeDocument = Join-Path $first 'palette.rprobe'
        [IO.File]::WriteAllText($probeDocument, 'palette')
        $script:dte.ItemOperations.OpenFile($probeDocument) | Out-Null
        Wait-For { Test-Path -LiteralPath ($paletteResult + '.ready') } 'palette package initialization'
    }
    if ($Palette) {
        Write-Output 'Closing palette source document'
        $script:dte.Documents.Item($main).Close(2)
        Write-Output 'Palette source document closed; reopening'
        $script:dte.ItemOperations.OpenFile($main) | Out-Null
        Write-Output 'Palette source document reopened'
        Wait-For { Test-Path -LiteralPath $paletteResult } 'effective editor palette'
        $observed = [IO.File]::ReadAllText($paletteResult)
        $required = @('sample {=rutile.namespace:#FF909090','Item {=rutile.type:#FF4EC9B0','value()=rutile.function:#FFDCDCAA','read(Item=rutile.function:#FFDCDCAA','executable;=rutile.function:#FFDCDCAA','i32 field=rutile.keyword:#FF569CD6','return this=rutile.control:#FFC586C0','cached-span-translation=True')
        if ($Typing) {
            $required += @('automatic-identifier-completion=True','native-brace-pair=True','native-brace-paired-backspace=True','native-brace-overtype=True','native-brace-return=True','native-brace-tab=True')
        }
        foreach ($expected in $required) {
            if (!$observed.Contains($expected)) {
                throw ('Effective editor palette mismatch: ' + $observed)
            }
        }
        $results.Add('effective editor palette and cached span translation')
        Write-Output 'effective editor palette and cached span translation: PASS'
    }
    if ($PaletteOnly) {
        return
    }


    $script:dte.Solution.Close($false)
    Wait-For { !(Managed $first) } 'old project owner shutdown'
    $script:dte.Solution.Open((Join-Path $second 'Rutile.sln'))
    $script:dte.ItemOperations.OpenFile((Join-Path $second 'app/main.rt')) | Out-Null
    Wait-For { Managed $second } 'replacement project owner startup'
    $script:dte.Solution.Close($false)
    Wait-For { !(Managed $second) } 'replacement project owner shutdown'
    $results.Add('solution root replacement and shutdown')
    Write-Output 'solution root replacement and shutdown: PASS'

}
finally {
    try {
        if ($script:dte) {
            foreach ($pane in $script:dte.ToolWindows.OutputWindow.OutputWindowPanes) {
                if ($pane.Name -match 'Rutile|Kiln') {
                    try {
                        $text = $pane.TextDocument.StartPoint.CreateEditPoint().GetText($pane.TextDocument.EndPoint)
                        [IO.File]::WriteAllText((Join-Path $output ($pane.Name + '-output.txt')), $text)
                    }
                    catch {
                        Write-Warning $_.Exception.Message
                    }
                }
            }
            try {
                $script:dte.Quit()
            }
            catch [Runtime.InteropServices.COMException] {
                Write-Warning ('Dedicated VS shutdown automation: ' + $_.Exception.Message)
            }
        }
    }
    finally {
        if ($compilerStartup) {
            Receive-Job $compilerStartup
            Stop-Job $compilerStartup
            Remove-Job $compilerStartup
        }
        if ($process -and !$process.HasExited -and !$process.WaitForExit(30000)) {
            $process.Kill()
        }
        if ($watchdog) {
            $watchdog.Dispose()
        }
        $filter.Dispose()
        @{ profile = $RootSuffix; version = $version; completed = $results.ToArray() } | ConvertTo-Json | Set-Content (Join-Path $output 'results.json')
    }
}
