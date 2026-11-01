# Clicks at a position relative to a process's main window (window coordinates as in capture_window.ps1 images).
# Usage: powershell -File tools\click_window.ps1 <process name> <x> <y>
param([Parameter(Mandatory)][string]$ProcessName, [Parameter(Mandatory)][int]$X, [Parameter(Mandatory)][int]$Y)

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class WinClick {
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
}
"@
[WinClick]::SetProcessDPIAware() | Out-Null
$p = Get-Process -Name $ProcessName -ErrorAction Stop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
[WinClick]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
Start-Sleep -Milliseconds 300
$r = New-Object WinClick+RECT
[WinClick]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
[WinClick]::SetCursorPos($r.Left + $X, $r.Top + $Y) | Out-Null
Start-Sleep -Milliseconds 150
[WinClick]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero)  # left down
Start-Sleep -Milliseconds 80
[WinClick]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)  # left up
"clicked at window ($X, $Y)"
