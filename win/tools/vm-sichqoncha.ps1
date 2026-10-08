# Sichqoncha tezlanishini oʻchiradi va 1:1 sezgirlikni qoʻyadi.
#
# NEGA: VM'ga sichqoncha hodisalari NISBIY (delta) koʻrinishida yetadi —
# QEMU'da usb-tablet va usb-mouse ikkalasi ham bor va faol boʻlgani
# nisbiy. Windows'ning «Enhance pointer precision» egri chizigʻi deltani
# kattalashtiradi, shuning uchun kursorni aniq nuqtaga olib borib
# boʻlmaydi. Tezlanish oʻchirilsa 1 delta = 1 piksel.
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Spi {
    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool SystemParametersInfo(uint a, uint b, int[] p, uint f);
    [DllImport("user32.dll", SetLastError=true)]
    public static extern bool SystemParametersInfoInt(uint a, uint b, IntPtr p, uint f);
}
"@
$mouse = @(0, 0, 0)                 # threshold1, threshold2, acceleration
[Spi]::SystemParametersInfo(0x0004, 0, $mouse, 3) | Out-Null      # SPI_SETMOUSE
[Spi]::SystemParametersInfoInt(0x0071, 0, [IntPtr]10, 3) | Out-Null  # SPI_SETMOUSESPEED
Set-ItemProperty 'HKCU:\Control Panel\Mouse' MouseSpeed 0
Set-ItemProperty 'HKCU:\Control Panel\Mouse' MouseThreshold1 0
Set-ItemProperty 'HKCU:\Control Panel\Mouse' MouseThreshold2 0
Set-ItemProperty 'HKCU:\Control Panel\Mouse' MouseSensitivity 10
Write-Output 'sichqoncha tezlanishi oʻchirildi'
