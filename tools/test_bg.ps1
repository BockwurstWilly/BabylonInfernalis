# Background test run of BabylonInfernalis: the window opens outside the desktop without focus (INDY64_BACKGROUND),
# the game starts by itself (INDY64_AUTOSTART) and inputs come from INDY64_AUTOINPUT inside the program, so nothing
# on the desktop is touched. Screenshots via PrintWindow.
# Usage: powershell -File tools\test_bg.ps1 -Name <n> -Seconds <s> [-Every <s>] [-Keys "<s>:<button>[/<ms>],..."]
#        [-BurstFrom <s> -BurstTo <s>]   (screenshots as fast as possible in that window, named <n>_b<ms>.png)
param([string]$Name = "bg", [int]$Seconds = 60, [int]$Every = 0, [string]$Keys = "", [int]$BurstFrom = -1, [int]$BurstTo = -1)

$root = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $root "indy64recomp"
$exe = Join-Path $dir "build\BabylonInfernalis.exe"
$crash = Join-Path $dir "crash.log"
New-Item -ItemType Directory -Force (Join-Path $dir "screens") | Out-Null
if (Test-Path $crash) { Remove-Item -LiteralPath $crash -Confirm:$false }
Get-Process -Name BabylonInfernalis -ErrorAction SilentlyContinue | Stop-Process -Force

$env:INDY64_BACKGROUND = "1"
$env:INDY64_AUTOSTART = "1"
$env:INDY64_AUTOINPUT = $Keys
$p = Start-Process -FilePath $exe -WorkingDirectory $dir -RedirectStandardOutput (Join-Path $dir "run_stdout.log") `
    -RedirectStandardError (Join-Path $dir "run_stderr.log") -PassThru
$capture = Join-Path $PSScriptRoot "capture_window.ps1"
$start = Get-Date
for ($t = 1; $t -le $Seconds -and -not $p.HasExited; $t++) {
    if (($t - 1) -ge $BurstFrom -and ($t - 1) -lt $BurstTo) {
        while (((Get-Date) - $start).TotalSeconds -lt $t) {
            $ms = [int]((Get-Date) - $start).TotalMilliseconds
            & powershell -NoProfile -ExecutionPolicy Bypass -File $capture BabylonInfernalis (Join-Path $dir "screens\$($Name)_b$ms.png") | Out-Null
        }
    }
    else {
        Start-Sleep -Seconds 1
    }
    if ($Every -gt 0 -and ($t % $Every) -eq 0 -and $t -lt $Seconds) {
        & powershell -NoProfile -ExecutionPolicy Bypass -File $capture BabylonInfernalis (Join-Path $dir "screens\$($Name)_$($t)s.png") | Out-Null
    }
}

if ($p.HasExited) {
    "exited with 0x{0:X8}" -f $p.ExitCode
}
else {
    & powershell -NoProfile -ExecutionPolicy Bypass -File $capture BabylonInfernalis (Join-Path $dir "screens\$Name.png")
    Stop-Process -Id $p.Id -Force
}
if (Test-Path $crash) { "--- crash.log"; Get-Content $crash }

