# Saves a screenshot of a process's main window (used to check BabylonInfernalis without a debugger).
# Usage: powershell -File tools\capture_window.ps1 <process name> <output.png>
param([Parameter(Mandatory)][string]$ProcessName, [Parameter(Mandatory)][string]$Out)

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
}
"@
[Win]::SetProcessDPIAware() | Out-Null
$p = Get-Process -Name $ProcessName -ErrorAction Stop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if (-not $p) { throw "no window for $ProcessName" }
$r = New-Object Win+RECT
[Win]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
$w = $r.Right - $r.Left; $h = $r.Bottom - $r.Top
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
if ($r.Left -lt -1000 -or $r.Top -lt -1000) {
    # Window placed outside the desktop (INDY64_BACKGROUND): ask it to draw itself, without bringing it to the front.
    $hdc = $g.GetHdc()
    [Win]::PrintWindow($p.MainWindowHandle, $hdc, 2) | Out-Null   # PW_RENDERFULLCONTENT
    $g.ReleaseHdc($hdc)
}
else {
    [Win]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
    Start-Sleep -Milliseconds 400
    $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
}
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
"saved $Out ($w x $h), title: $($p.MainWindowTitle)"
