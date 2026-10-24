@echo off
rem Builds into build\. Usage: build.bat [research]
rem Without arguments builds the mod as build\nofalldamage.asi.
rem "research" adds SetHealth logging (RESEARCH define) and builds build\nofalldamage_research.asi.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set SRC=%~dp0
set MH=%SRC%..\..\third_party\minhook
set MAIN=%SRC%nofalldamage.cpp
set OUT=nofalldamage.asi
set DEFS=
if /i "%1"=="research" set OUT=nofalldamage_research.asi
if /i "%1"=="research" set DEFS=/DRESEARCH
if not exist "%SRC%build" mkdir "%SRC%build"
cl /nologo /O2 /MT /EHsc /W3 /LD %DEFS% /I"%MH%\include" ^
  "%MAIN%" "%MH%\src\hook.c" "%MH%\src\buffer.c" "%MH%\src\trampoline.c" "%MH%\src\hde\hde64.c" ^
  /Fo"%SRC%build\\" /Fe"%SRC%build\%OUT%" ^
  /link user32.lib || exit /b 1
echo Built %SRC%build\%OUT% from %MAIN%
