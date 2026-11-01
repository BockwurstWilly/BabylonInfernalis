# Syntax-checks the RT64 files touched for the Factor 5 port with clang (-fsyntax-only), without building RT64.
# Needs the header-only submodules hlslpp, plume and xxHash under third_party\rt64\src\contrib.
# Usage: powershell -File tools\check_rt64_syntax.ps1 [files relative to third_party\rt64\src ...]
#        -Root <rt64 dir>   check another RT64 tree (default third_party\rt64)
param([string[]]$Files = @("gbi/rt64_gbi_f5indi.cpp", "gbi/rt64_gbi.cpp", "hle/rt64_interpreter.cpp", "hle/rt64_rsp.cpp"),
      [string]$Root = "")

$rt = if ($Root) { $Root } else { Join-Path (Split-Path -Parent $PSScriptRoot) "third_party\rt64" }
$clang = "C:\Program Files\LLVM\bin\clang++.exe"
$inc = @("src", "src/contrib", "src/contrib/plume", "src/contrib/imgui", "src/contrib/hlslpp/include",
         "src/contrib/mupen64plus-core/src/api", "src/contrib/xxHash") | ForEach-Object { "-I" + (Join-Path $rt $_) }
$inc += "-I" + (Join-Path (Split-Path -Parent $PSScriptRoot) "third_party\SDL2\include")
$inc += "-I" + (Join-Path $rt "src\contrib\mupen64plus-win32-deps\SDL2-2.26.3\include")
$failed = 0
foreach ($f in $Files) {
    $out = & $clang -fsyntax-only -std=c++17 -w -ferror-limit=20 -DHLSL_CPU -DNOMINMAX -D__PRFCHWINTRIN_H "-msse4.1" @inc (Join-Path $rt "src\$f") 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -eq 0) { "OK    $f" } else { $failed++; "FAIL  $f"; $out | Select-Object -First 25 }
}
exit $failed
