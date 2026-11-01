# Regenerates the N64Recomp inputs from the Ghidra project and runs the recompiler.
# Usage: powershell -File tools\run_recomp.ps1 [-Rebuild]   (-Rebuild: rebuild the Ghidra project first)
param([switch]$Rebuild)

$root = Split-Path -Parent $PSScriptRoot
$py = "python"
if ($Rebuild) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "rebuild_ghidra.ps1") | Out-Null
}
$env:JAVA_HOME = "C:\Program Files\Eclipse Adoptium\jdk-21.0.11.10-hotspot"
$env:TEMP = $env:TMP = "D:\Tools\ghidra-mcp\tmp"
& "D:\Tools\ghidra_12.1.4_PUBLIC\support\analyzeHeadless.bat" (Join-Path $root "analysis\ghidra") indy64 `
    -process indy_boot.bin -noanalysis -readOnly -scriptPath (Join-Path $root "tools\ghidra") `
    -postScript ExportFunctions.java (Join-Path $root "analysis\functions.csv") | Out-Null
& $py -I (Join-Path $root "tools\make_recomp_syms.py") (Join-Path $root "analysis\functions.csv") (Join-Path $root "recomp\indy64.syms.toml")
# The recompiler is the N64Recomp submodule of indy64recomp's N64ModernRuntime (tools\build_n64recomp_runtime.cmd),
# so the generated code matches the runtime it is linked against.
$n64recompSrc = Join-Path $root "indy64recomp\lib\N64ModernRuntime\N64Recomp"
& $py -I (Join-Path $root "tools\make_recomp_config.py") (Join-Path $root "rom\indy.z64") (Join-Path $root "recomp") `
    (Join-Path $n64recompSrc "src\symbol_lists.cpp")
Push-Location (Join-Path $root "recomp")
& (Join-Path $root "third_party\N64Recomp-runtime-build\N64Recomp.exe") indy64.recomp.toml 2>&1 | ForEach-Object { "$_" } |
    Tee-Object -FilePath recomp_run.log | Select-Object -Last 15
$code = $LASTEXITCODE
Pop-Location
"N64Recomp exit code: $code"
