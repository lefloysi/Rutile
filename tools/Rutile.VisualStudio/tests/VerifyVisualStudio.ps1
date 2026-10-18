param(
    [Parameter(Mandatory = $true)] [string]$Vsix,
    [switch]$Install,
    [string]$RootSuffix,
    [string]$Kiln
)

$ErrorActionPreference = 'Stop'
if ([Threading.Thread]::CurrentThread.GetApartmentState() -ne 'STA') {
    throw 'Run this verification with powershell.exe -STA.'
}
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$version = [IO.File]::ReadAllText((Join-Path $root 'version/release.txt')).Trim()
if (!$RootSuffix) {
    $RootSuffix = 'RutileValidation' + ($version -split '\.')[-1]
}
if ($RootSuffix -notmatch '^RutileValidation[0-9]+$') {
    throw 'Use a dedicated RutileValidation<number> profile; normal and Exp profiles are forbidden.'
}
$Vsix = [IO.Path]::GetFullPath($Vsix)
if (!$Kiln) {
    $Kiln = Join-Path $root 'build/release/bin/kiln.exe'
}
$Kiln = [IO.Path]::GetFullPath($Kiln)
if ((& $Kiln --version | Out-String).Trim() -ne $version) {
    throw 'The verification Kiln does not match version/release.txt.'
}
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
        throw 'The VSIX does not match version/release.txt.'
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
if ($Install) {
    $installer = [Diagnostics.ProcessStartInfo]::new((Join-Path $installation.installationPath 'Common7/IDE/VSIXInstaller.exe'))
    $installer.UseShellExecute = $false
    $installer.CreateNoWindow = $true
    $installer.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $installer.Arguments = '/quiet /rootSuffix:' + $RootSuffix + ' /instanceids:' + $installation.instanceId + ' "' + $Vsix + '"'
    $deployment = [Diagnostics.Process]::Start($installer)
    if (!$deployment.WaitForExit(120000) -or $deployment.ExitCode) {
        throw 'Installing the VSIX into the dedicated profile failed.'
    }
    $deployment.Dispose()
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
    [IO.File]::WriteAllText((Join-Path $directory 'rutile.yaml'), "targets:`n  app:`n    source: app`n    entry: main`n")
    [IO.File]::WriteAllText((Join-Path $directory 'Rutile.sln'), @'
Microsoft Visual Studio Solution File, Format Version 12.00
# Visual Studio Version 17
Project("{2150E333-8FDC-42A3-9474-1A3956D46DE8}") = "Sources", "Sources", "{7EFDBB69-D15E-4901-9021-026FB246BC30}"
    ProjectSection(SolutionItems) = preProject
        app\main.rt = app\main.rt
        app\library.rt = app\library.rt
        rutile.yaml = rutile.yaml
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
        return $status -match '(?m)^Rutile online\s*$' -and $status -match 'managed by the IDE'
    }
    finally {
        Pop-Location
    }
}

Add-Type -Path (Join-Path $PSScriptRoot 'VisualStudioAutomation.cs')
$filter = [VisualStudioMessageFilter]::new()
$first = New-Project 'first'
$second = New-Project 'second'
$process = $null
$script:dte = $null
$results = [Collections.Generic.List[string]]::new()
try {
    $launch = [Diagnostics.ProcessStartInfo]::new((Join-Path $installation.installationPath 'Common7/IDE/devenv.exe'))
    $launch.UseShellExecute = $false
    $launch.CreateNoWindow = $true
    $launch.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $launch.WorkingDirectory = $output
    $launch.Arguments = '/RootSuffix ' + $RootSuffix + ' /Embedding /Log "' + (Join-Path $output 'ActivityLog.xml') + '"'
    $process = [Diagnostics.Process]::Start($launch)
    $watchdog = [OwnedProcessTimeout]::new($process, 180000)
    Wait-For { $script:dte = [VisualStudioAutomation]::Find($process.Id); $script:dte -ne $null } 'dedicated Visual Studio automation'
    Write-Output 'Dedicated automation object: attached'
    $script:dte.SuppressUI = $true
    $script:dte.MainWindow.Visible = $false
    Write-Output 'Opening dedicated test solution'
    $script:dte.Solution.Open((Join-Path $first 'Rutile.sln'))
    $main = Join-Path $first 'app/main.rt'
    $script:dte.ItemOperations.OpenFile($main) | Out-Null
    Wait-For { Managed $first } 'IDE-owned Kiln startup'
    $results.Add('IDE-owned project process')
    Write-Output 'IDE-owned project process: PASS'

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
            $script:dte.Quit()
        }
    }
    finally {
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
