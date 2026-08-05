# Launch the per-desk background upload server (crop + QR) and open Tommy's URL.
$ErrorActionPreference = "Stop"
$FirmwareRoot = Split-Path $PSScriptRoot -Parent
$PyCandidates = @(
  "$env:USERPROFILE\.pyenv\pyenv-win\versions\3.13.0\python.exe",
  "$env:USERPROFILE\.pyenv\pyenv-win\versions\3.14.6\python.exe",
  "python"
)
$Python = $null
foreach ($c in $PyCandidates) {
  if ($c -eq "python") {
    $Python = "python"
    break
  }
  if (Test-Path $c) {
    $Python = $c
    break
  }
}
if (-not $Python) {
  Write-Error "Python not found (need pillow + qrcode)."
}

& $Python -c "import qrcode, PIL" 2>$null
if ($LASTEXITCODE -ne 0) {
  Write-Host "Installing pillow + qrcode…"
  & $Python -m pip install pillow qrcode
}

$Desk = if ($args.Count -gt 0) { $args[0] } else { "mac-tommy" }
Write-Host "Starting background upload server for $Desk …"
& $Python (Join-Path $PSScriptRoot "bg_upload_server.py") --desk $Desk
