@echo off
setlocal
cd /d "%~dp0"
if exist "mrs_audio_check.exe" (
  "mrs_audio_check.exe" --interactive
) else (
  "build\Release\mrs_audio_check.exe" --interactive
)
echo.
pause
