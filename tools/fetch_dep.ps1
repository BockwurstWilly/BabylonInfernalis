# Downloads a GitHub repository at a fixed commit (zip, no git needed) into a target folder, replacing it.
# Usage: powershell -File tools\fetch_dep.ps1 <owner/repo> <commit> <target dir> [exclude patterns for tar ...]
# Extraction uses Windows' tar into a short temp folder, since some repositories exceed MAX_PATH inside long paths;
# exclude patterns (e.g. "*/shaders*") skip files that are not needed for building.
param([Parameter(Mandatory)][string]$Repo, [Parameter(Mandatory)][string]$Commit, [Parameter(Mandatory)][string]$Target,
      [string[]]$Exclude = @())

$ProgressPreference = 'SilentlyContinue'
$dl = Join-Path (Split-Path -Parent $PSScriptRoot) "third_party\_dl"
New-Item -ItemType Directory -Force $dl | Out-Null
$zip = Join-Path $dl ((($Repo -replace '/', '_') + "_" + $Commit.Substring(0, 7)) + ".zip")
if (-not (Test-Path $zip)) {
    Invoke-WebRequest -UseBasicParsing "https://codeload.github.com/$Repo/zip/$Commit" -OutFile $zip
}
$tmp = "D:\_x_" + $Commit.Substring(0, 7)
if (Test-Path $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force -Confirm:$false }
New-Item -ItemType Directory -Force $tmp | Out-Null
$args = @("-xf", $zip, "-C", $tmp)
foreach ($e in $Exclude) { $args += @("--exclude", $e) }
& tar.exe @args
if ($LASTEXITCODE -ne 0) { throw "tar failed for $Repo" }
if (Test-Path $Target) { Remove-Item -LiteralPath $Target -Recurse -Force -Confirm:$false }
New-Item -ItemType Directory -Force (Split-Path -Parent $Target) | Out-Null
Move-Item (Get-ChildItem $tmp -Directory | Select-Object -First 1).FullName $Target
Remove-Item -LiteralPath $tmp -Recurse -Force -Confirm:$false
"{0,-45} {1,10:N0} bytes -> {2}" -f $Repo, (Get-Item $zip).Length, $Target
