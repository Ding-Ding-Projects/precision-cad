@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\bootstrap-website.ps1"
exit /b %errorlevel%
