@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Benchmark-Threads.ps1" %*
if errorlevel 1 pause
