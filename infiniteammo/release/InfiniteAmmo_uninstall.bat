@echo off
rem Removes a manual (non-Vortex) InfiniteAmmo install from the Mad Max game folder. Double-click to run.
rem Deletes infiniteammo.asi, infiniteammo.ini and infiniteammo.log. Optional argument: the game folder.
setlocal
title InfiniteAmmo uninstall

call :find_game "%~1" || goto :fail

tasklist /fi "imagename eq MadMax.exe" /nh 2>nul | find /i "MadMax.exe" >nul
if not errorlevel 1 (
  echo Mad Max is running. Quit the game and run this file again.
  goto :fail
)

findstr /i /c:"infiniteammo.asi" "%GAME%\vortex.deployment.json" >nul 2>&1
if not errorlevel 1 (
  echo InfiniteAmmo is installed by Vortex. Disable or remove it in Vortex instead.
  goto :fail
)

set "FOUND="
for %%f in (infiniteammo.asi infiniteammo.ini infiniteammo.log) do if exist "%GAME%\%%f" (
  del /f "%GAME%\%%f"
  if exist "%GAME%\%%f" (
    echo Could not delete %%f. Run this file as administrator.
    goto :fail
  )
  echo Deleted %%f
  set "FOUND=1"
)
echo.
if defined FOUND (
  echo InfiniteAmmo is removed from "%GAME%".
) else (
  echo InfiniteAmmo is not installed in "%GAME%".
)
pause
exit /b 0

:fail
echo.
echo Uninstall failed.
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
