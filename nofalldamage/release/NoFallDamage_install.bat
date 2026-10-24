@echo off
rem Installs NoFallDamage into the Mad Max game folder without Vortex. Double-click to run.
rem Extracts nofalldamage.asi from the newest NoFallDamage-*.zip next to this file.
rem Optional argument: the game folder.
setlocal
title NoFallDamage install
set "SRC=%~dp0"

set "ZIP="
for /f "delims=" %%z in ('dir /b /on "%SRC%NoFallDamage-*.zip" 2^>nul') do set "ZIP=%SRC%%%z"
if not defined ZIP (
  echo No NoFallDamage-*.zip was found next to this file.
  goto :fail
)

call :find_game "%~1" || goto :fail

tasklist /fi "imagename eq MadMax.exe" /nh 2>nul | find /i "MadMax.exe" >nul
if not errorlevel 1 (
  echo Mad Max is running. Quit the game and run this file again.
  goto :fail
)

findstr /i /c:"nofalldamage.asi" "%GAME%\vortex.deployment.json" >nul 2>&1
if not errorlevel 1 (
  echo NoFallDamage is installed by Vortex. Disable it in Vortex first, then run this file again.
  goto :fail
)

"%SystemRoot%\System32\tar.exe" -xf "%ZIP%" -C "%GAME%" nofalldamage.asi
if errorlevel 1 (
  echo Could not extract the files to "%GAME%". Run this file as administrator.
  goto :fail
)

if not exist "%GAME%\dinput8.dll" (
  echo.
  echo WARNING: dinput8.dll is not in the game folder. Install Ultimate ASI Loader x64 as dinput8.dll, or the mod does not load.
)
echo.
for %%z in ("%ZIP%") do echo Installed %%~nxz in "%GAME%".
pause
exit /b 0

:fail
echo.
echo Install failed.
pause
exit /b 1

rem Sets GAME to the Mad Max folder: argument, Steam registry, default Steam path, then asks.
:find_game
set "GAME=%~1"
if not defined GAME for /f "tokens=2,*" %%a in ('reg query "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Steam App 234140" /v InstallLocation /reg:64 2^>nul ^| find "InstallLocation"') do set "GAME=%%b"
if not defined GAME set "GAME=%ProgramFiles(x86)%\Steam\steamapps\common\Mad Max"
if exist "%GAME%\MadMax.exe" exit /b 0
echo MadMax.exe was not found in "%GAME%".
set /p "GAME=Enter the game folder (the folder with MadMax.exe): "
set "GAME=%GAME:"=%"
if exist "%GAME%\MadMax.exe" exit /b 0
echo MadMax.exe was not found in "%GAME%".
exit /b 1
