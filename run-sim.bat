@echo off
set "PATH=C:\msys64\mingw64\bin;%PATH%"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0firmware\scripts\run-sim.ps1"

