@echo off
setlocal
rem Use the Windows PowerShell inbox modules in this child only.  A PowerShell 7
rem parent can otherwise pass a PSModulePath that hides Get-FileHash on 5.1.
set "PSModulePath=%ProgramFiles%\WindowsPowerShell\Modules;%SystemRoot%\System32\WindowsPowerShell\v1.0\Modules"
set "silentArgument="
set "forwardedArguments=%*"
if /I "%~1"=="/s" (
    set "silentArgument=-Silent"
    set "forwardedArguments=%~2 %~3 %~4 %~5 %~6 %~7 %~8 %~9"
) else if /I "%~1"=="--silent" (
    set "silentArgument=-Silent"
    set "forwardedArguments=%~2 %~3 %~4 %~5 %~6 %~7 %~8 %~9"
)
if "%SILENT%"=="1" set "silentArgument=-Silent"
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-native-installer.ps1" %silentArgument% %forwardedArguments%
exit /b %errorlevel%
