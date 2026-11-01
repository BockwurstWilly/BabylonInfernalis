# Compresses a dl_<n>.txt dump (INDY64_DUMP_DL) into the sequence of interesting F5 commands with repeat counts.
# Usage: powershell -File tools\dl_summary.ps1 <dl file> [-Max <chars>]
param([Parameter(Mandatory)][string]$File, [int]$Max = 4000)

$lines = Get-Content $File | Where-Object { $_ -notmatch '^\s+sub' }
$seq = New-Object System.Collections.Generic.List[string]
$prev = ''
$n = 0
foreach ($x in $lines) {
    $parts = $x -split ' '
    $w0 = $parts[1]
    $w1 = $parts[2]
    $op = $w0.Substring(0, 2)
    $cls = switch -regex ($op) {
        '^(B4|BF)$' { 'TRI' }
        '^E4$' { 'TEXRECT' }
        '^(FF|FE|F6|F7|ED)$' { "${op}:${w1}" }
        '^01$' {
            if ($w0 -eq '0105C03F') { 'MTX' }
            elseif ($w0 -eq '0101300F') { 'VIEWPORT' }
            elseif ($w0 -like '010E*') { 'VTXGENMTX' }
            else { '.' }
        }
        '^05$' { "GEN:$($w1.Substring(6))" }
        '^BC$' { if ($w0 -match 'BC00012[8C]') { "OFS:$($w0.Substring(4)):${w1}" } else { '.' } }
        default { '.' }
    }
    if ($cls -eq '.') { continue }
    if ($cls -eq $prev) { $n++ }
    else {
        if ($prev) { $seq.Add("$prev x$n") }
        $prev = $cls
        $n = 1
    }
}
$seq.Add("$prev x$n")
"lines $($lines.Count)"
$text = $seq -join ' | '
$text.Substring(0, [Math]::Min($Max, $text.Length))
