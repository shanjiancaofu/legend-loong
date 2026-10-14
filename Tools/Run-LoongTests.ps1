#Requires -Version 7.0
[CmdletBinding()]
param(
    [ValidateSet('Quick', 'Combat', 'Elements', 'Building', 'Grid', 'Persistence', 'Camera', 'Full')]
    [string]$Group = 'Full',
    [string]$TestFilter,
    [string]$EngineRoot,
    [string]$ProjectPath,
    [string]$EditorTarget,
    [string]$ReportPath,
    [string[]]$ChangedModules = @(),
    [switch]$SkipBuild,
    [ValidateRange(10, 7200)][int]$TestTimeoutSeconds = 300,
    [ValidateRange(10, 7200)][int]$BuildTimeoutSeconds = 1800
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Testing/AutomationReport.ps1')
. (Join-Path $PSScriptRoot 'Testing/Process.ps1')

function Resolve-LoongEngine {
    param([string]$ExplicitRoot, [string]$Association)
    $candidates = @()
    if ($ExplicitRoot) { $candidates = @($ExplicitRoot) }
    elseif ($env:UE_ENGINE_ROOT) { $candidates = @($env:UE_ENGINE_ROOT) }
    else {
        if ($Association -match '^\d+\.\d+$') {
            $install = Get-ItemProperty "HKLM:/SOFTWARE/EpicGames/Unreal Engine/$Association" -ErrorAction SilentlyContinue
            if ($install.InstalledDirectory) { $candidates += $install.InstalledDirectory }
        }
        $sourceBuilds = Get-ItemProperty 'HKCU:/SOFTWARE/Epic Games/Unreal Engine/Builds' -ErrorAction SilentlyContinue
        if ($sourceBuilds -and $sourceBuilds.PSObject.Properties[$Association]) {
            $candidates += $sourceBuilds.PSObject.Properties[$Association].Value
        }
        $launcherFile = Join-Path $env:ProgramData 'Epic/UnrealEngineLauncher/LauncherInstalled.dat'
        if (Test-Path -LiteralPath $launcherFile) {
            $launcher = Get-Content -LiteralPath $launcherFile -Raw | ConvertFrom-Json
            $candidates += @($launcher.InstallationList | Where-Object AppName -eq "UE_$Association" | ForEach-Object InstallLocation)
        }
        $candidates += @(Get-Process UnrealEditor -ErrorAction SilentlyContinue | ForEach-Object {
            if ($_.Path) { [IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $_.Path) '../../..')) }
        })
    }
    foreach ($candidate in @($candidates | Select-Object -Unique)) {
        $root = [IO.Path]::GetFullPath($candidate)
        $versionFile = Join-Path $root 'Engine/Build/Build.version'
        if (-not (Test-Path -LiteralPath $versionFile)) { continue }
        $version = Get-Content -LiteralPath $versionFile -Raw | ConvertFrom-Json
        $compatible = $Association -notmatch '^\d+\.\d+$' -or
                      $Association -eq "$($version.MajorVersion).$($version.MinorVersion)"
        if ($compatible -and (Test-Path -LiteralPath (Join-Path $root 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'))) {
            return $root
        }
    }
    throw "No matching UE installation for '$Association'. Supply -EngineRoot or UE_ENGINE_ROOT."
}

$workspace = Split-Path -Parent $PSScriptRoot
if (-not $ProjectPath) {
    $projects = @(Get-ChildItem -LiteralPath $workspace -Filter '*.uproject' -File)
    if ($projects.Count -ne 1) { throw 'Specify -ProjectPath when the workspace does not contain exactly one uproject.' }
    $ProjectPath = $projects[0].FullName
}
$ProjectPath = (Resolve-Path -LiteralPath $ProjectPath).Path
$projectRoot = Split-Path -Parent $ProjectPath
$project = Get-Content -LiteralPath $ProjectPath -Raw | ConvertFrom-Json
$EngineRoot = Resolve-LoongEngine $EngineRoot $project.EngineAssociation
$version = Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
if (-not $EditorTarget) {
    $targets = @(Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Source') -Filter '*Editor.Target.cs' -File)
    if ($targets.Count -ne 1) { throw 'Specify -EditorTarget when discovery is ambiguous.' }
    $EditorTarget = $targets[0].Name -replace '\.Target\.cs$', ''
}
$groups = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Testing/TestGroups.json') -Raw | ConvertFrom-Json
$prefixes = @($groups.$Group)
$runGroup = $Group
if ($TestFilter) {
    if ($TestFilter -notmatch '^[A-Za-z0-9_.+]+$') { throw 'TestFilter accepts Loong test names/prefixes separated by +.' }
    $prefixes = @($TestFilter.Split('+'))
    $runGroup = 'Custom'
}
foreach ($prefix in $prefixes) {
    if (-not $prefix.StartsWith('Loong.', [StringComparison]::Ordinal)) { throw 'Only Loong tests may be selected.' }
}
$registered = @()
foreach ($source in Get-ChildItem -LiteralPath (Join-Path $projectRoot 'Source') -Filter '*.cpp' -Recurse -File) {
    $registrationMatches = [regex]::Matches([IO.File]::ReadAllText($source.FullName), 'IMPLEMENT_SIMPLE_AUTOMATION_TEST\s*\(\s*\w+\s*,\s*"([^"]+)"')
    foreach ($match in $registrationMatches) { $registered += $match.Groups[1].Value }
}
$expected = @($registered | Where-Object {
    $name = $_
    @($prefixes | Where-Object { $name.StartsWith($_, [StringComparison]::Ordinal) }).Count -gt 0
} | Sort-Object -Unique)
if (-not $expected.Count) { throw "No source tests match '$($prefixes -join '+')'." }
$filter = ($prefixes | ForEach-Object { '^' + $_ }) -join '+'
if (-not $ReportPath) { $ReportPath = Join-Path $projectRoot 'Saved/Automation' }
elseif (-not [IO.Path]::IsPathRooted($ReportPath)) { $ReportPath = Join-Path $projectRoot $ReportPath }
$runPath = Join-Path ([IO.Path]::GetFullPath($ReportPath)) ($runGroup + '_' + (Get-Date -Format 'yyyyMMdd_HHmmss') + '_' + [guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $runPath -Force | Out-Null
$lockRoot = Join-Path $projectRoot 'Saved/Automation'
New-Item -ItemType Directory -Path $lockRoot -Force | Out-Null
$lock = [IO.File]::Open((Join-Path $lockRoot 'LoongRunner.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
$summary = [ordered]@{
    Status = 'Running'; Project = $ProjectPath; EngineRoot = $EngineRoot
    EngineVersion = "$($version.MajorVersion).$($version.MinorVersion).$($version.PatchVersion)"
    EngineChangelist = $version.Changelist
    PowerShellVersion = $PSVersionTable.PSVersion.ToString()
    EditorTarget = $EditorTarget; Group = $runGroup; Filter = $filter; ChangedModules = @($ChangedModules)
    ExpectedTests = $expected; BuildSkipped = [bool]$SkipBuild; ModuleSuffix = $null
    ReportDirectory = $runPath; Passed = 0; Failed = 0; Skipped = 0; TestWarnings = 0; Error = $null
}
$summary.SourceChanges = @(& git -C $projectRoot diff --name-only HEAD -- Source Config) +
                         @(& git -C $projectRoot ls-files --others --exclude-standard -- Source Config)
$saveDirectory = Join-Path $projectRoot 'Saved/SaveGames'
$saveHashes = @{}
if (Test-Path -LiteralPath $saveDirectory) {
    Get-ChildItem -LiteralPath $saveDirectory -Filter '*.sav' -File | ForEach-Object {
        $saveHashes[$_.FullName] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
}
Write-Host "UE $($summary.EngineVersion): $EngineRoot"
Write-Host "$runGroup : $($expected.Count) expected tests. Reports: $runPath"
try {
    if (-not $SkipBuild) {
        $dotnets = @(Get-ChildItem -LiteralPath (Join-Path $EngineRoot 'Engine/Binaries/ThirdParty/DotNet') -Filter 'dotnet.exe' -Recurse -File)
        if ($dotnets.Count -ne 1) { throw 'Cannot uniquely resolve the engine bundled dotnet.exe.' }
        $ubt = Join-Path $EngineRoot 'Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll'
        $buildArgs = @($ubt, $EditorTarget, 'Win64', 'Development', "-Project=$ProjectPath", '-WaitMutex', '-NoHotReloadFromIDE', "-Log=$runPath/Build.log")
        $activeEditor = @(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" -ErrorAction SilentlyContinue | Where-Object {
            $_.CommandLine -and $_.CommandLine.Replace('/', '\').IndexOf(
                $ProjectPath.Replace('/', '\'), [StringComparison]::OrdinalIgnoreCase) -ge 0
        })
        if ($activeEditor.Count) {
            $suffix = Get-Random -Minimum 2000 -Maximum 65000
            foreach ($module in $project.Modules) {
                while (Test-Path -LiteralPath (Join-Path $projectRoot "Binaries/Win64/UnrealEditor-$($module.Name)-$suffix.dll")) { ++$suffix }
                $buildArgs += "-ModuleWithSuffix=$($module.Name),$suffix"
            }
            $summary.ModuleSuffix = $suffix
            Write-Host "Editor is open: compiling an independent module suffix ($suffix). Restart Editor to use changes in PIE."
        }
        $summary.BuildExitCode = Invoke-LoongProcess $dotnets[0].FullName $buildArgs $projectRoot (Join-Path $runPath 'Build-process.txt') $BuildTimeoutSeconds
        if ($summary.BuildExitCode -ne 0) { throw "Editor build failed: $($summary.BuildExitCode). Read $runPath/Build.log" }
    }
    $startedUtc = [DateTime]::UtcNow
    $editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    $testArgs = @($ProjectPath, '-unattended', '-nop4', '-nosplash', '-nullrhi', '-nosound',
                  "-ExecCmds=Automation RunTests $filter", '-TestExit=Automation Test Queue Empty',
                  "-ReportExportPath=$runPath", "-AbsLog=$runPath/Unreal.log")
    $summary.TestExitCode = Invoke-LoongProcess $editor $testArgs $projectRoot (Join-Path $runPath 'Editor-process.txt') $TestTimeoutSeconds
    if ($summary.TestExitCode -ne 0) { throw "Editor/test process failed: $($summary.TestExitCode). Read $runPath/Unreal.log" }
    $reportFile = Join-Path $runPath 'index.json'
    $rawReport = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
    $summary.Passed = $rawReport.succeeded
    $summary.Failed = $rawReport.failed
    $summary.Skipped = $rawReport.notRun
    $summary.TestWarnings = $rawReport.succeededWithWarnings
    $report = Read-LoongAutomationReport $reportFile $expected $startedUtc
    $summary.Passed = $report.succeeded
    $summary.Status = 'Passed'
}
catch {
    $summary.Status = 'Failed'
    $summary.Error = $_.Exception.Message
}
finally {
    $savesUnchanged = $true
    foreach ($path in $saveHashes.Keys) {
        if (-not (Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $saveHashes[$path]) {
            $savesUnchanged = $false
            $summary.Status = 'Failed'
            $summary.Error = "Existing save changed during run: $path. No automatic restore performed."
        }
    }
    if (Test-Path -LiteralPath $saveDirectory) {
        $leftovers = @(Get-ChildItem -LiteralPath $saveDirectory -Filter '*.sav' -File | Where-Object {
            -not $saveHashes.ContainsKey($_.FullName) -and $_.Name -match '^Loong_(QA|Automation)_[0-9a-f]{32}.*\.sav$'
        })
        if ($leftovers.Count) { $summary.Status = 'Failed'; $summary.Error = "Test save cleanup incomplete: $($leftovers.Name -join ', ')" }
    }
    $summary.SaveHashesUnchanged = $savesUnchanged
    $summary | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $runPath 'summary.json') -Encoding UTF8
    $lock.Dispose()
}
if ($summary.Status -ne 'Passed') { throw "$($summary.Error) Summary: $runPath/summary.json" }
Write-Host "PASS: $($summary.Passed) tests. Existing saves unchanged. Report: $runPath/index.html"
