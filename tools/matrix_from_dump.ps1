# Lists the F5 force-matrix commands (0x0105C03F) found in an RDRAM dump (big-endian, as written by
# Project64's mem.getblock) and decodes the 4x4 fixed-point matrices they point to.
# Usage: powershell -File tools\matrix_from_dump.ps1 <dump.bin> [-Max <n>]
param([Parameter(Mandatory)][string]$Dump, [int]$Max = 12)

$b = [System.IO.File]::ReadAllBytes($Dump)
function U32([int]$o) { return ([uint32]$b[$o] -shl 24) -bor ([uint32]$b[$o + 1] -shl 16) -bor ([uint32]$b[$o + 2] -shl 8) -bor [uint32]$b[$o + 3] }
function S16([int]$o) { $v = ([int]$b[$o] -shl 8) -bor $b[$o + 1]; if ($v -ge 0x8000) { $v -= 0x10000 }; return $v }
function U16([int]$o) { return ([int]$b[$o] -shl 8) -bor $b[$o + 1] }

$seen = @{}
for ($o = 0; $o -lt $b.Length - 8; $o += 8) {
    if ((U32 $o) -ne 0x0105C03F) { continue }
    $addr = (U32 ($o + 4)) -band 0xFFFFFF
    if ($seen.ContainsKey($addr) -or $addr + 0x40 -gt $b.Length) { continue }
    $seen[$addr] = $true
    "cmd at {0:X6} -> matrix {1:X6}" -f $o, $addr
    for ($r = 0; $r -lt 4; $r++) {
        $row = for ($c = 0; $c -lt 4; $c++) {
            $i = $r * 4 + $c
            (S16 ($addr + $i * 2)) + (U16 ($addr + 0x20 + $i * 2)) / 65536.0
        }
        "    " + (($row | ForEach-Object { "{0,12:F5}" -f $_ }) -join " ")
    }
    if ($seen.Count -ge $Max) { break }
}
