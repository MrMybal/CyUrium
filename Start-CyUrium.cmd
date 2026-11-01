@echo off
if exist "%~dp0dist\windows\CyUrium\CyUrium.exe" (
  start "" /D "%~dp0" "%~dp0dist\windows\CyUrium\CyUrium.exe" --profile "%~dp0.runtime\profiles\default"
  exit /b 0
)
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\build.ps1" -Run
if errorlevel 1 pause
