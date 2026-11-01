# Starts each given level in a background run (GameShark "start on level" code via INDY64_GS) and keeps the
# screenshots and logs per level: screens\lvl<NN>*.png, levels\lvl<NN>_stderr.log, levels\lvl<NN>_crash.log.
# Usage: powershell -File tools\test_levels.ps1 [-Levels 2,3,...] [-Seconds 130]
param([int[]]$Levels = (2..15), [int]$Seconds = 130)

$root = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $root "indy64recomp"
$out = Join-Path $dir "levels"
New-Item -ItemType Directory -Force $out | Out-Null
$keys = "36:START,44:START,52:START,60:START,68:START,76:A,84:A"

foreach ($level in $Levels) {
    $hex = "{0:X2}" -f $level
    $env:INDY64_GS = "D0080C37 0000,80080C37 00$hex"
    $result = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "test_bg.ps1") -Name "lvl$hex" -Seconds $Seconds -Every 20 -Keys $keys
    Copy-Item (Join-Path $dir "run_stderr.log") (Join-Path $out "lvl$($hex)_stderr.log") -Force
    $crash = Join-Path $dir "crash.log"
    if (Test-Path $crash) { Copy-Item $crash (Join-Path $out "lvl$($hex)_crash.log") -Force }
    "level $hex done: $(($result | Out-String).Trim())"
}
Remove-Item Env:INDY64_GS
