# Drive the LVGL sim over TCP (WERKPAGER_DRIVE=1).
#   powershell -File firmware/scripts/drive-sim.ps1 -Start
#   powershell -File firmware/scripts/drive-sim.ps1 -Cmd "tap 240 240","wait 400","shot hub","screen"
#   powershell -File firmware/scripts/drive-sim.ps1 -Script path\to\commands.txt

param(
  [switch]$Start,
  [switch]$Build,
  [string[]]$Cmd = @(),
  [string]$Script = "",
  [int]$Port = 9471
)

$ErrorActionPreference = "Stop"
$Mingw = "C:\msys64\mingw64\bin"
$env:Path = "$Mingw;" + $env:Path
$Firmware = Split-Path $PSScriptRoot -Parent
$BuildDir = Join-Path $Firmware "build"
$Exe = Join-Path $BuildDir "werkpager_sim.exe"

if ($Build -or -not (Test-Path $Exe)) {
  Push-Location $BuildDir
  try {
    if (-not (Test-Path "build.ninja")) { cmake -G Ninja -DCMAKE_BUILD_TYPE=Release .. }
    cmake --build .
  } finally { Pop-Location }
}

if ($Start) {
  Stop-Process -Name werkpager_sim -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 300
  $env:WERKPAGER_DRIVE = "1"
  $env:WERKPAGER_DRIVE_PORT = "$Port"
  Remove-Item Env:WERKPAGER_SHOT -ErrorAction SilentlyContinue
  Remove-Item Env:WERKPAGER_SCREEN -ErrorAction SilentlyContinue
  Start-Process -FilePath $Exe -WorkingDirectory $BuildDir
  Start-Sleep -Milliseconds 1800
  Write-Host "Sim started with DRIVE on port $Port"
}

$lines = @()
if ($Script -and (Test-Path $Script)) {
  $lines = Get-Content $Script | Where-Object { $_ -and ($_ -notmatch '^\s*#') }
}
$lines += $Cmd
if ($lines.Count -eq 0) {
  Write-Host "No commands. Use -Cmd or -Script."
  return
}

$client = New-Object System.Net.Sockets.TcpClient
$deadline = [datetime]::UtcNow.AddSeconds(8)
while (-not $client.Connected -and [datetime]::UtcNow -lt $deadline) {
  try { $client.Connect("127.0.0.1", $Port) } catch { Start-Sleep -Milliseconds 200 }
}
if (-not $client.Connected) { throw "Could not connect to drive port $Port. Start sim with -Start." }

$stream = $client.GetStream()
$reader = New-Object System.IO.StreamReader($stream)
$writer = New-Object System.IO.StreamWriter($stream)
$writer.AutoFlush = $true
$stream.ReadTimeout = 15000

# banner
try { Write-Host ("<< " + $reader.ReadLine()) } catch {}

foreach ($line in $lines) {
  Write-Host (">> " + $line)
  # playthrough can run many minutes against the Will bot
  if ($line -match '^\s*playthrough\b') { $stream.ReadTimeout = 600000 }
  else { $stream.ReadTimeout = 15000 }
  $writer.WriteLine($line)
  $resp = $reader.ReadLine()
  Write-Host ("<< " + $resp)
  if ($resp -notmatch '^OK') { throw "Drive command failed: $line -> $resp" }
}

$client.Close()
