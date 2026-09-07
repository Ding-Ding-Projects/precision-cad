@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\bootstrap-website.ps1" -InstallOnly
exit /b %errorlevel%
