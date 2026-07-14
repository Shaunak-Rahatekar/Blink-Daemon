$pngPath = "C:\Users\SHAUNAK\.gemini\antigravity-ide\brain\a77f6eb5-fb75-4ed5-b168-60250a9de5f1\blink_daemon_logo_1784035247682.png"
$icoPath = "d:\Blink Daemon\icon.ico"

$pngBytes = [System.IO.File]::ReadAllBytes($pngPath)
$icoBytes = New-Object System.Collections.Generic.List[byte]

# ICONDIR
$icoBytes.AddRange([byte[]](0, 0, 1, 0, 1, 0))

# ICONDIRENTRY (256x256 image, represented as 0,0)
$icoBytes.AddRange([byte[]](0, 0, 0, 0, 1, 0, 32, 0))

# Size of PNG data
$icoBytes.AddRange([System.BitConverter]::GetBytes([int]$pngBytes.Length))

# Offset of PNG data (22 bytes for header)
$icoBytes.AddRange([System.BitConverter]::GetBytes([int]22))

# PNG Data
$icoBytes.AddRange($pngBytes)

[System.IO.File]::WriteAllBytes($icoPath, $icoBytes.ToArray())
Write-Host "Successfully converted to icon.ico"
