@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-native.ps1" %*
exit /b %errorlevel%
