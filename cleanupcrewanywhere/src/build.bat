@echo off
rem Builds build\cleanupcrewanywhere.asi.
setlocal
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set SRC=%~dp0
set MH=%SRC%..\..\third_party\minhook
set MAIN=%SRC%cleanupcrewanywhere.cpp
set OUT=cleanupcrewanywhere.asi
if not exist "%SRC%build" mkdir "%SRC%build"
cl /nologo /O2 /MT /EHsc /W3 /LD /I"%MH%\include" ^
  "%MAIN%" "%MH%\src\hook.c" "%MH%\src\buffer.c" "%MH%\src\trampoline.c" "%MH%\src\hde\hde64.c" ^
  /Fo"%SRC%build\\" /Fe"%SRC%build\%OUT%" ^
  /link user32.lib || exit /b 1
echo Built %SRC%build\%OUT% from %MAIN%
