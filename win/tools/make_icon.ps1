# assets/icon_1024.png dan Windows .ico yasaydi.
#
# ICO ichiga PNG siqilgan tasvirlar joylanadi — Windows Vista'dan beri
# qo'llab-quvvatlanadi va katta o'lchamlar uchun ancha ixcham.
#
# Ishlatish: powershell -File make_icon.ps1

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$src  = Join-Path $root 'assets\icon_1024.png'
$dst  = Join-Path $root 'win\res\AppIcon.ico'

New-Item -ItemType Directory -Force (Split-Path $dst -Parent) | Out-Null

# Windows shu o'lchamlarni turli joylarda ishlatadi:
# 16 - sarlavha/tray, 32 - ish stoli, 48 - Explorer, 256 - katta ko'rinish.
$sizes = @(16, 24, 32, 48, 64, 128, 256)

$source = [System.Drawing.Image]::FromFile($src)
$pngs = @()

foreach ($s in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap $s, $s, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.SmoothingMode      = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.DrawImage($source, (New-Object System.Drawing.Rectangle 0, 0, $s, $s))
    $g.Dispose()

    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngs += ,@($s, $ms.ToArray())
    $ms.Dispose(); $bmp.Dispose()
}
$source.Dispose()

$out = [System.IO.File]::Create($dst)
$w = New-Object System.IO.BinaryWriter $out

# ICONDIR
$w.Write([uint16]0)              # zaxira
$w.Write([uint16]1)              # tur: 1 = ikonka
$w.Write([uint16]$pngs.Count)

# Har bir tasvir ma'lumotlari ICONDIRENTRY massividan keyin keladi
$offset = 6 + 16 * $pngs.Count

foreach ($p in $pngs) {
    $size = $p[0]; $data = $p[1]
    # 256 o'lchami baytda 0 sifatida yoziladi
    $w.Write([byte]($(if ($size -ge 256) { 0 } else { $size })))   # kenglik
    $w.Write([byte]($(if ($size -ge 256) { 0 } else { $size })))   # balandlik
    $w.Write([byte]0)            # palitra ranglari (0 = palitrasiz)
    $w.Write([byte]0)            # zaxira
    $w.Write([uint16]1)          # rang tekisliklari
    $w.Write([uint16]32)         # piksel uchun bit
    $w.Write([uint32]$data.Length)
    $w.Write([uint32]$offset)
    $offset += $data.Length
}

foreach ($p in $pngs) { $w.Write($p[1]) }

$w.Flush(); $w.Close(); $out.Close()

$kb = [math]::Round((Get-Item $dst).Length / 1KB, 1)
Write-Output "Yaratildi: $dst ($kb KB, $($pngs.Count) o'lcham)"
