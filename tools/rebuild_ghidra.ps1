# Rebuilds the Ghidra project analysis\ghidra\indy64 from the ROM and applies symbols\symbols.csv.
# Usage: powershell -File tools\rebuild_ghidra.ps1 [-SymbolsOnly]
#   -SymbolsOnly  keep the existing project and only re-apply symbols.csv
param([switch]$SymbolsOnly)

$root = Split-Path -Parent $PSScriptRoot
$ghidra = "D:\Tools\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat"
$proj = Join-Path $root "analysis\ghidra"
$rom = Join-Path $root "rom\indy.z64"
$boot = Join-Path $root "analysis\indy_boot.bin"
$scripts = Join-Path $root "tools\ghidra"
$syms = Join-Path $root "symbols\symbols.csv"
$env:JAVA_HOME = "C:\Program Files\Eclipse Adoptium\jdk-21.0.11.10-hotspot"
$env:TEMP = $env:TMP = "D:\Tools\ghidra-mcp\tmp"   # short TEMP path, see D:\Tools\ghidra-mcp\README.md

if ($SymbolsOnly) {
    & $ghidra $proj indy64 -process indy_boot.bin -noanalysis -scriptPath $scripts -postScript ApplySymbols.java $syms
    exit $LASTEXITCODE
}

# Boot segment = ROM 0x1000-0x4DC30, loaded at 0x40000400.
$bytes = [IO.File]::ReadAllBytes($rom)
$seg = New-Object byte[] (0x4DC30 - 0x1000)
[Array]::Copy($bytes, 0x1000, $seg, 0, $seg.Length)
[IO.File]::WriteAllBytes($boot, $seg)

foreach ($p in "indy64.rep", "indy64.gpr") {
    $t = Join-Path $proj $p
    if (Test-Path -LiteralPath $t) { Remove-Item -LiteralPath $t -Recurse -Force -Confirm:$false }
}
New-Item -ItemType Directory -Force $proj | Out-Null
& $ghidra $proj indy64 -import $boot -loader BinaryLoader -loader-baseAddr 0x40000400 `
    -processor "MIPS:BE:64:64-32addr" -cspec o32 -scriptPath $scripts `
    -preScript N64Setup.java $rom -postScript ApplySymbols.java $syms -postScript FillGaps.java `
    -postScript Report.java `
    -analysisTimeoutPerFile 3000
exit $LASTEXITCODE
