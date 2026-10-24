@echo off
rem Builds build\fovchanger.asi. Needs VS 2022 C++.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set SRC=%~dp0
if not exist "%SRC%build" mkdir "%SRC%build"
cl /nologo /O2 /MT /EHsc /W3 /LD "%SRC%fovchanger.cpp" ^
  /Fo"%SRC%build\\" /Fe"%SRC%build\fovchanger.asi" ^
  /link user32.lib || exit /b 1
echo Built %SRC%build\fovchanger.asi
