# Benchmark TurboOCR HTTP OCR endpoint and process resource usage.
#
# The target app must already have HTTP enabled in its config.ini, e.g.:
#   [http]
#   enabled=true
#   port=19527
#   lan_access=false
#
# Example:
#   powershell -ExecutionPolicy Bypass -File tools/bench_ocr.ps1 `
#     -Exe "F:\QPrj\TurboOCRV1.1.0\dist\TurboOCR-1.1.0\TurboOCR.exe" `
#     -Image "F:\QPrj\TurboOCRV1.1.0\demo\封面.png" `
#     -Label "ORT DirectML (medium)" -Runs 10 -Warmup 2 -WaitSeconds 120

param(
    [Parameter(Mandatory=$true)][string]$Exe,
    [Parameter(Mandatory=$true)][string]$Image,
    [string]$Label = "run",
    [int]$Runs = 10,
    [int]$Warmup = 2,
    [int]$Port = 19527,
    [int]$WaitSeconds = 120,
    [string]$OutFile = ""
)

$ErrorActionPreference = "Stop"

function Get-GpuUsedMB {
    try {
        $v = & nvidia-smi --query-gpu=memory.used --format=csv,noheader,nounits 2>$null | Select-Object -First 1
        if ($v) { return [double]($v.Trim()) }
    } catch {}
    return $null
}

function Get-ProcessGpuMB([int]$procId) {
    try {
        $lines = & nvidia-smi --query-compute-apps=pid,used_memory --format=csv,noheader,nounits 2>$null
        foreach ($line in $lines) {
            $parts = $line -split ","
            if ($parts.Count -ge 2 -and [int]($parts[0].Trim()) -eq $procId) {
                return [double]($parts[1].Trim())
            }
        }
    } catch {}
    return $null
}

function Get-Stats($values) {
    $a = @($values | Sort-Object)
    $n = $a.Count
    if ($n -eq 0) { return $null }
    if ($n % 2 -eq 1) {
        $median = $a[[int]($n / 2)]
    } else {
        $median = ($a[$n / 2 - 1] + $a[$n / 2]) / 2.0
    }
    $p95idx = [Math]::Min($n - 1, [Math]::Max(0, [int][Math]::Ceiling(0.95 * $n) - 1))
    return [pscustomobject]@{
        min    = [Math]::Round($a[0], 2)
        median = [Math]::Round($median, 2)
        mean   = [Math]::Round((($a | Measure-Object -Average).Average), 2)
        p95    = [Math]::Round($a[$p95idx], 2)
        max    = [Math]::Round($a[$n - 1], 2)
    }
}

$gpuBase = Get-GpuUsedMB
$p = Start-Process -FilePath $Exe -PassThru

try {
    $ready = $false
    $deadline = (Get-Date).AddSeconds($WaitSeconds)
    while ((Get-Date) -lt $deadline) {
        if ($p.HasExited) {
            throw "Process exited with code $($p.ExitCode) before HTTP was ready"
        }
        try {
            $r = Invoke-WebRequest -Uri "http://127.0.0.1:$Port/api/health" `
                -UseBasicParsing -TimeoutSec 3
            if ($r.StatusCode -eq 200) { $ready = $true; break }
        } catch {
            Start-Sleep -Milliseconds 500
        }
    }
    if (-not $ready) {
        throw "HTTP not ready within $WaitSeconds seconds"
    }
    Start-Sleep -Seconds 1

    for ($i = 0; $i -lt $Warmup; $i++) {
        Invoke-WebRequest -Uri "http://127.0.0.1:$Port/api/ocr/image" `
            -Method Post -InFile $Image -UseBasicParsing -TimeoutSec 300 | Out-Null
    }

    $proc = Get-Process -Id $p.Id
    $proc.Refresh()
    $cpu0 = $proc.TotalProcessorTime.TotalSeconds
    $warmWs = $proc.WorkingSet64
    $warmPrivate = $proc.PrivateMemorySize64
    $gpuWarm = Get-GpuUsedMB
    $procGpu = Get-ProcessGpuMB $p.Id

    $wallSw = [Diagnostics.Stopwatch]::StartNew()
    $engineMs = @()
    $wallMs = @()
    $lastCount = 0

    for ($i = 0; $i -lt $Runs; $i++) {
        $sw = [Diagnostics.Stopwatch]::StartNew()
        $resp = Invoke-WebRequest -Uri "http://127.0.0.1:$Port/api/ocr/image" `
            -Method Post -InFile $Image -UseBasicParsing -TimeoutSec 300
        $sw.Stop()
        $wallMs += [Math]::Round($sw.Elapsed.TotalMilliseconds, 2)
        $json = $resp.Content | ConvertFrom-Json
        $engineMs += [double]$json.total_ms
        $lastCount = [int]$json.count
    }
    $wallSw.Stop()

    $proc.Refresh()
    $cpu1 = $proc.TotalProcessorTime.TotalSeconds
    $wsEnd = $proc.WorkingSet64
    $privateEnd = $proc.PrivateMemorySize64
    $peakWs = $proc.PeakWorkingSet64
    $gpuEnd = Get-GpuUsedMB

    $wallSec = $wallSw.Elapsed.TotalSeconds
    $cpuDelta = $cpu1 - $cpu0
    $logical = [Environment]::ProcessorCount
    $avgCores = if ($wallSec -gt 0) { $cpuDelta / $wallSec } else { 0 }

    $result = [pscustomobject]@{
        label                     = $Label
        exe                       = $Exe
        image                     = $Image
        runs                      = $Runs
        warmup                    = $Warmup
        engine_ms                 = Get-Stats $engineMs
        wall_ms                   = Get-Stats $wallMs
        cpu_avg_cores             = [Math]::Round($avgCores, 2)
        cpu_avg_percent_total     = [Math]::Round(100 * $avgCores / $logical, 1)
        cpu_avg_percent_one_core  = [Math]::Round(100 * $avgCores, 1)
        cpu_time_s                = [Math]::Round($cpuDelta, 2)
        wall_s                    = [Math]::Round($wallSec, 2)
        working_set_mb            = [Math]::Round($wsEnd / 1MB, 1)
        peak_working_set_mb       = [Math]::Round($peakWs / 1MB, 1)
        private_mb                = [Math]::Round($privateEnd / 1MB, 1)
        warm_working_set_mb       = [Math]::Round($warmWs / 1MB, 1)
        warm_private_mb           = [Math]::Round($warmPrivate / 1MB, 1)
        gpu_baseline_mb           = $gpuBase
        gpu_after_warmup_mb       = $gpuWarm
        gpu_end_mb                = $gpuEnd
        gpu_delta_after_warmup_mb = if ($null -ne $gpuWarm -and $null -ne $gpuBase) { [Math]::Round($gpuWarm - $gpuBase, 1) } else { $null }
        gpu_process_mb            = $procGpu
        last_count                = $lastCount
        logical_cpus              = $logical
    }

    $result | ConvertTo-Json -Depth 6
    if ($OutFile -ne "") {
        $result | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 $OutFile
    }
}
finally {
    if ($p -and -not $p.HasExited) {
        Stop-Process -Id $p.Id -Force
    }
}
