@echo off
setlocal
rem Use the Windows PowerShell inbox modules in this child only.  A PowerShell 7
rem parent can otherwise pass a PSModulePath that hides Get-FileHash on 5.1.
set "PSModulePath=%ProgramFiles%\WindowsPowerShell\Modules;%SystemRoot%\System32\WindowsPowerShell\v1.0\Modules"
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-native.ps1" %*
exit /b %errorlevel%
