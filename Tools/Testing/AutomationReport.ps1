function Read-LoongAutomationReport {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)][string[]]$ExpectedTests,
        [Parameter(Mandatory)][DateTime]$StartedUtc
    )
    $file = Get-Item -LiteralPath $Path -ErrorAction Stop
    if ($file.LastWriteTimeUtc -lt $StartedUtc) { throw "Stale automation report: $Path" }
    $report = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    $actual = @($report.tests | ForEach-Object { $_.fullTestPath })
    if ($actual.Count -eq 0) { throw 'No tests were discovered/run.' }
    $missing = @($ExpectedTests | Where-Object { $actual -cnotcontains $_ })
    if ($missing.Count) { throw "Tests not discovered/run: $($missing -join ', ')" }
    if (@($actual | Select-Object -Unique).Count -ne $actual.Count) { throw 'Duplicate test results.' }
    $bad = @($report.tests | Where-Object { $_.state -ne 'Success' -or $_.errors -gt 0 -or $_.warnings -gt 0 })
    if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.inProcess -ne 0 -or
        $report.succeededWithWarnings -ne 0 -or $report.succeeded -ne $actual.Count -or $bad.Count) {
        foreach ($test in $bad) {
            Write-Host "$($test.fullTestPath): $($test.state)"
            foreach ($entry in $test.entries) {
                if ($entry.event.type -in @('Error', 'Warning')) { Write-Host "  $($entry.event.message)" }
            }
        }
        throw "Automation failed/incomplete: passed=$($report.succeeded), failed=$($report.failed), skipped=$($report.notRun)"
    }
    return $report
}
