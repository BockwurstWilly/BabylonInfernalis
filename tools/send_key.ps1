# Presses a key in a process's main window (brings it to the front first).
# Usage: powershell -File tools\send_key.ps1 <process name> <key: Enter|Space|Up|Down|Left|Right|Escape|Z|X> [hold ms]
param([Parameter(Mandatory)][string]$ProcessName, [Parameter(Mandatory)][string]$Key, [int]$HoldMs = 120)

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class WinKey {
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint type);
}
"@
$codes = @{ Enter = 0x0D; Space = 0x20; Up = 0x26; Down = 0x28; Left = 0x25; Right = 0x27; Escape = 0x1B; Z = 0x5A; X = 0x58;
    W = 0x57; A = 0x41; S = 0x53; D = 0x44; E = 0x45; F = 0x46; R = 0x52; Q = 0x51; LShift = 0xA0 }
$vk = [byte]$codes[$Key]
$extended = if ($Key -in @('Up', 'Down', 'Left', 'Right')) { 1 } else { 0 }
$scan = [byte][WinKey]::MapVirtualKey($vk, 0)
$p = Get-Process -Name $ProcessName -ErrorAction Stop | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
[WinKey]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
Start-Sleep -Milliseconds 150
[WinKey]::keybd_event($vk, $scan, $extended, [UIntPtr]::Zero)
Start-Sleep -Milliseconds $HoldMs
[WinKey]::keybd_event($vk, $scan, 2 -bor $extended, [UIntPtr]::Zero)
