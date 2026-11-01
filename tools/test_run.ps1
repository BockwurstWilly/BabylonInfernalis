# One automated test run of BabylonInfernalis: start, click "Start Game", wait, screenshot, report crash.log.
# Usage: powershell -File tools\test_run.ps1 [-Name <screenshot name>] [-Seconds <wait after start>] [-Every <seconds>]
#        [-Keys "<s>:<key>,..."] [-KeepRunning]
param([string]$Name = "run", [int]$Seconds = 15, [int]$Every = 0, [string]$Keys = "", [switch]$KeepRunning)

$root = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $root "indy64recomp"
$exe = Join-Path $dir "build\BabylonInfernalis.exe"
$crash = Join-Path $dir "crash.log"
New-Item -ItemType Directory -Force (Join-Path $dir "screens") | Out-Null
if (Test-Path $crash) { Remove-Item -LiteralPath $crash -Confirm:$false }
Get-Process -Name BabylonInfernalis -ErrorAction SilentlyContinue | Stop-Process -Force

$env:INDY64_SAMPLE = "4"
$env:INDY64_TRACE_MQ = "1"
$sample = Join-Path $dir "sample.log"
if (Test-Path $sample) { Remove-Item -LiteralPath $sample -Confirm:$false }
$p = Start-Process -FilePath $exe -WorkingDirectory $dir -RedirectStandardOutput (Join-Path $dir "run_stdout.log") `
    -RedirectStandardError (Join-Path $dir "run_stderr.log") -PassThru
Start-Sleep -Seconds 6
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "click_window.ps1") BabylonInfernalis 810 428 | Out-Null
# Second-by-second loop: key presses from -Keys ("<s>:<key>,...", see send_key.ps1) and, with -Every,
# intermediate screenshots <Name>_<seconds>s.png.
$keyEvents = @{}
if ($Keys) {
    foreach ($k in $Keys -split ',') {
        $parts = $k.Trim() -split ':'
        $keyEvents[[int]$parts[0]] = @($keyEvents[[int]$parts[0]]) + $parts[1] | Where-Object { $_ }
    }
}
for ($t = 1; $t -le $Seconds -and -not $p.HasExited; $t++) {
    Start-Sleep -Seconds 1
    foreach ($key in @($keyEvents[$t])) {
        if ($key) {
            # "<key>/<hold ms>" holds the key; long holds run in the background so the loop keeps its timing.
            $keyParts = $key -split '/'
            $hold = if ($keyParts.Count -gt 1) { [int]$keyParts[1] } else { 120 }
            $keyArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $PSScriptRoot "send_key.ps1"), 'BabylonInfernalis', $keyParts[0], $hold)
            if ($hold -gt 800) { Start-Process powershell -ArgumentList $keyArgs -WindowStyle Hidden | Out-Null }
            else { & powershell @keyArgs | Out-Null }
        }
    }
    if ($Every -gt 0 -and ($t % $Every) -eq 0 -and $t -lt $Seconds) {
        & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "capture_window.ps1") BabylonInfernalis (Join-Path $dir "screens\$($Name)_$($t)s.png") | Out-Null
    }
}

if ($p.HasExited) {
    "exited with 0x{0:X8}" -f $p.ExitCode
}
else {
    $shot = Join-Path $dir "screens\$Name.png"
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "capture_window.ps1") BabylonInfernalis $shot
    if (-not $KeepRunning) { Stop-Process -Id $p.Id -Force }
}

if (Test-Path $crash) { "--- crash.log"; Get-Content $crash }
"--- stderr (tail)"; Get-Content (Join-Path $dir "run_stderr.log") -Tail 15
