#Requires -Version 7.0
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'AutomationReport.ps1')
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$directory = Join-Path $projectRoot ('Saved/Automation/RunnerValidation_' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory -Force | Out-Null
$path = Join-Path $directory 'index.json'
$started = [DateTime]::UtcNow.AddSeconds(-1)
$expected = @('Loong.Test.One', 'Loong.Test.Two')
function New-Report {
    return @{
        succeeded = 2; succeededWithWarnings = 0; failed = 0; notRun = 0; inProcess = 0
        tests = @(@{ fullTestPath = $expected[0]; state = 'Success'; errors = 0; warnings = 0 },
                  @{ fullTestPath = $expected[1]; state = 'Success'; errors = 0; warnings = 0 })
    }
}
function Assert-Rejected {
    param($Report, [string]$Reason)
    $Report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $path -Encoding UTF8
    $rejected = $false
    try { Read-LoongAutomationReport $path $expected $started | Out-Null }
    catch { $rejected = $true }
    if (-not $rejected) { throw "Report validation accepted: $Reason" }
    Write-Host "PASS: rejects $Reason"
}
$report = New-Report
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $path -Encoding UTF8
Read-LoongAutomationReport $path $expected $started | Out-Null
Write-Host 'PASS: accepts complete successful report'
$report = New-Report
$report.failed = 1; $report.succeeded = 1; $report.tests[0].state = 'Fail'; $report.tests[0].errors = 1
Assert-Rejected $report 'failed assertion even if the process returned zero'
$report = New-Report
$report.tests = @($report.tests[0]); $report.succeeded = 1
Assert-Rejected $report 'missing expected test / stale binary'
$report = New-Report
$report.notRun = 1; $report.succeeded = 1; $report.tests[0].state = 'NotRun'
Assert-Rejected $report 'skipped test'
$report = New-Report
$report.tests[0].warnings = 1; $report.succeeded = 1; $report.succeededWithWarnings = 1
Assert-Rejected $report 'test warning'
$report = New-Report
$report.tests = @(); $report.succeeded = 0
Assert-Rejected $report 'empty discovery'
$report = New-Report
$report.tests += $report.tests[0]; $report.succeeded = 3
Assert-Rejected $report 'duplicate test results'
$report = New-Report
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $path -Encoding UTF8
[IO.File]::SetLastWriteTimeUtc($path, $started.AddMinutes(-1))
$rejected = $false
try { Read-LoongAutomationReport $path $expected $started | Out-Null }
catch { $rejected = $true }
if (-not $rejected) { throw 'Accepted stale report' }
Write-Host 'PASS: rejects stale report'
