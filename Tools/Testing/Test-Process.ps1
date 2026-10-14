#Requires -Version 7.0
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Process.ps1')
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$directory = Join-Path $projectRoot ('Saved/Automation/ProcessValidation_' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $directory -Force | Out-Null
$pwshPath = (Get-Process -Id $PID).Path
$exit = Invoke-LoongProcess $pwshPath @('-NoProfile', '-Command', 'exit 7') $projectRoot (Join-Path $directory 'Exit.txt') 10
if ($exit -ne 7) { throw 'Lost nonzero process exit code.' }
Write-Host 'PASS: preserves nonzero process exit code'
$timeout = $false
try {
    # Wait on an unsignalled condition; the parent deadline must terminate this owned child.
    $condition = '$condition = [Threading.ManualResetEvent]::new($false); [void]$condition.WaitOne()'
    Invoke-LoongProcess $pwshPath @('-NoProfile', '-Command', $condition) $projectRoot (Join-Path $directory 'Timeout.txt') 1 | Out-Null
}
catch {
    if ($_.Exception.Message -notlike 'Timeout after 1s.*') { throw }
    $timeout = $true
}
if (-not $timeout) { throw 'Missing timeout failure.' }
if (-not (Test-Path -LiteralPath (Join-Path $directory 'Timeout.txt'))) { throw 'Timeout did not retain process output.' }
Write-Host 'PASS: timeout kills owned child and retains diagnostic output'
