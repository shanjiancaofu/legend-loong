param(
    [string]$EngineRoot = 'D:/soft/epic/UE_5.8',
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectPath = Join-Path $projectRoot 'Loong.uproject'
$projectInfo = Get-Content -LiteralPath $projectPath -Raw | ConvertFrom-Json
$engineInfo = Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
$engineAssociation = '{0}.{1}' -f $engineInfo.MajorVersion, $engineInfo.MinorVersion
if ($projectInfo.EngineAssociation -ne $engineAssociation) {
    throw "Engine mismatch: project=$($projectInfo.EngineAssociation), engine=$engineAssociation"
}
$logRoot = Join-Path $projectRoot 'Saved/Logs'
$reportRoot = Join-Path $projectRoot 'Saved/Automation/P0-01A'
New-Item -ItemType Directory -Path $logRoot -Force | Out-Null
if (-not $SkipBuild) {
    & (Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat') LoongEditor Win64 Development `
        "-Project=$projectPath" -WaitMutex -NoHotReloadFromIDE "-Log=$logRoot/P0-01A-EditorBuild.log"
    if ($LASTEXITCODE -ne 0) { throw "Editor build failed: $LASTEXITCODE" }
}
$startedUtc = [DateTime]::UtcNow
& (Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe') $projectPath `
    -unattended -nop4 -nosplash -nullrhi -nosound '-ExecCmds=Automation RunTests Loong.' `
    '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$reportRoot" "-AbsLog=$logRoot/P0-01A-Tests.log"
if ($LASTEXITCODE -ne 0) { throw "Unreal automation process failed: $LASTEXITCODE" }
# UE TestExit can return 0 even when assertions fail. The fresh JSON report is authoritative.
$reportPath = Join-Path $reportRoot 'index.json'
$reportFile = Get-Item -LiteralPath $reportPath
if ($reportFile.LastWriteTimeUtc -lt $startedUtc) { throw 'Automation report is stale.' }
$report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0 -or
    $report.succeededWithWarnings -ne 0 -or $report.tests.Count -lt 12) {
    $report.tests | Where-Object state -ne 'Success' | Select-Object fullTestPath, state | Format-Table
    throw "Automation incomplete/failed: passed=$($report.succeeded), failed=$($report.failed), skipped=$($report.notRun)"
}
Write-Output "PASS: $($report.succeeded) tests. Report: $reportPath"
