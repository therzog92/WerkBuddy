# Build + run the LVGL PC simulator (480x480 SDL window).
#   powershell -File firmware/scripts/run-sim.ps1
#   powershell -File firmware/scripts/run-sim.ps1 -Shot   # save sim-out/preview.png and exit

param(
  [switch]$Shot,
  [switch]$ConfigureOnly
)

$ErrorActionPreference = "Stop"
$Mingw = "C:\msys64\mingw64\bin"
if (-not (Test-Path "$Mingw\gcc.exe")) {
  Write-Error "MinGW not found at $Mingw. Install MSYS2 + mingw-w64-x86_64-gcc/SDL2 first."
}
$env:Path = "$Mingw;" + $env:Path

$Firmware = Split-Path $PSScriptRoot -Parent
$Build = Join-Path $Firmware "build"
$Lvgl = Join-Path $Firmware "third_party\lvgl"

if (-not (Test-Path (Join-Path $Lvgl "CMakeLists.txt"))) {
  Write-Host "Cloning LVGL release/v9.3..."
  New-Item -ItemType Directory -Force -Path (Join-Path $Firmware "third_party") | Out-Null
  git clone --depth 1 --branch release/v9.3 https://github.com/lvgl/lvgl.git $Lvgl
}

New-Item -ItemType Directory -Force -Path $Build | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Firmware "sim-out") | Out-Null

Push-Location $Build
try {
  if (-not (Test-Path "build.ninja") -and -not (Test-Path "Makefile")) {
    cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
  }
  if ($ConfigureOnly) { return }
  cmake --build .
  $exe = Join-Path $Build "werkpager_sim.exe"
  if (-not (Test-Path $exe)) { Write-Error "Missing $exe" }

  if ($Shot) {
    $env:WERKPAGER_SHOT = "1"
    & $exe
    $preview = Join-Path $Firmware "sim-out\preview.png"
    if (Test-Path $preview) {
      Write-Host "Preview: $preview"
    } else {
      Write-Warning "No preview.png written"
    }
  } else {
    Remove-Item Env:WERKPAGER_SHOT -ErrorAction SilentlyContinue
    & $exe
  }
} finally {
  Pop-Location
}
