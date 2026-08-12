# Thin wrapper — prefer the Python driver (binary shot + pyserial).
#   powershell -File firmware/scripts/drive-device.ps1 -Port COM5 -Cmd "ping","shot hub"
param(
  [Parameter(Mandatory = $true)][string]$Port,
  [string[]]$Cmd = @(),
  [int]$Baud = 921600
)
$ErrorActionPreference = "Stop"
$Py = Join-Path $PSScriptRoot "drive-device.py"
if ($Cmd.Count -eq 0) { throw "Use -Cmd with one or more drive commands" }
& python $Py --port $Port --baud $Baud @Cmd
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
