function Invoke-LoongProcess {
    param([string]$Executable, [string[]]$Arguments, [string]$WorkingDirectory,
          [string]$OutputPath, [int]$TimeoutSeconds)
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $Executable
    $info.WorkingDirectory = $WorkingDirectory
    $info.UseShellExecute = $false
    $info.CreateNoWindow = $true
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $info
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $nextProgress = 30
    try {
        [void]$process.Start()
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $timedOut = $false
        while (-not $process.WaitForExit(1000)) {
            if ($watch.Elapsed.TotalSeconds -ge $TimeoutSeconds) {
                # Only terminate this runner's newly launched process tree, never an existing editor.
                $process.Kill($true)
                $timedOut = $true
                break
            }
            if ($watch.Elapsed.TotalSeconds -ge $nextProgress) {
                Write-Host ("Waiting for {0}: {1:N0}s" -f [IO.Path]::GetFileName($Executable), $watch.Elapsed.TotalSeconds)
                $nextProgress += 30
            }
        }
        $process.WaitForExit()
        ($stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()) |
            Set-Content -LiteralPath $OutputPath -Encoding UTF8
        if ($timedOut) { throw "Timeout after ${TimeoutSeconds}s. Process output: $OutputPath" }
        return $process.ExitCode
    }
    finally { $process.Dispose() }
}
