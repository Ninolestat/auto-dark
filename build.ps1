$ErrorActionPreference = "Stop"

$zig = (Get-Command zig -ErrorAction SilentlyContinue).Source
if (-not $zig) {
    $zig = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Filter "zig.exe" -Recurse -ErrorAction SilentlyContinue |
           Select-Object -First 1 -ExpandProperty FullName
}
if (-not $zig -or -not (Test-Path $zig)) {
    throw "zig.exe non trovato."
}

New-Item -ItemType Directory -Force -Path "build" | Out-Null

Write-Host "1. Compilazione AutoDark.exe..."
& $zig cc -target x86_64-windows-gnu -Os -s -Wall -Wextra `
    src/main.c src/sun.c src/theme.c src/icon.c `
    -o build/AutoDark.exe `
    "-Wl,--subsystem,windows" -municode `
    -ldwmapi -lshell32 -luser32 -lgdi32 -ladvapi32 -lcomdlg32

if ($LASTEXITCODE -ne 0) { throw "Errore compilazione AutoDark.exe" }

Write-Host "2. Generazione build/payload.h..."
$bytes = [System.IO.File]::ReadAllBytes("build/AutoDark.exe")
$sb = New-Object System.Text.StringBuilder ($bytes.Length * 5 + 256)
[void]$sb.AppendLine("#ifndef AUTODARK_PAYLOAD_H")
[void]$sb.AppendLine("#define AUTODARK_PAYLOAD_H")
[void]$sb.AppendLine("static const unsigned int g_payload_len = $($bytes.Length);")
[void]$sb.Append("static const unsigned char g_payload[] = {")
for ($i = 0; $i -lt $bytes.Length; $i++) {
    if ($i % 24 -eq 0) { [void]$sb.Append("`n    ") }
    [void]$sb.Append("0x$($bytes[$i].ToString('X2'))")
    if ($i -lt $bytes.Length - 1) { [void]$sb.Append(",") }
}
[void]$sb.AppendLine("`n};")
[void]$sb.AppendLine("#endif")
[System.IO.File]::WriteAllText("build/payload.h", $sb.ToString(), [System.Text.Encoding]::ASCII)

Write-Host "3. Compilazione AutoDarkSetup.exe..."
& $zig cc -target x86_64-windows-gnu -Os -s -Wall -Wextra `
    src/installer.c src/icon.c `
    -o build/AutoDarkSetup.exe `
    "-Wl,--subsystem,windows" -municode `
    -ldwmapi -lshell32 -luser32 -lgdi32 -ladvapi32 -lcomdlg32 -lole32 -luuid

if ($LASTEXITCODE -ne 0) { throw "Errore compilazione AutoDarkSetup.exe" }

Get-Item build/AutoDark.exe, build/AutoDarkSetup.exe | Select-Object Name, Length
