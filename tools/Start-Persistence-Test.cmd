@echo off
cd /d "%~dp0"
mrs_persistence_check.exe
if errorlevel 1 echo Persistence check failed.
pause
