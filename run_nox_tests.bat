@echo off
rem Runs every tests\*.nox normally AND compiled with noxc, and compares. Output: run_log.txt
setlocal
set "ROOT=%~dp0"
set "SRC=%ROOT%src"
set "LOG=%ROOT%run_log.txt"
if not exist "%SRC%\nox.exe" (echo nox.exe not found - run build_nox.bat first> "%LOG%" & exit /b 1)
cd /d "%ROOT%tests" || (echo tests folder not found> "%LOG%" & exit /b 1)
if not exist compiled mkdir compiled
> "%LOG%" echo Nox test run %DATE% %TIME%
for %%F in (*.nox) do call :one "%%~nF"
echo.>> "%LOG%"
echo ===== error handling =====>> "%LOG%"
echo --- noxc -p bad\syntax_error.nox>> "%LOG%"
"%SRC%\noxc.exe" -p "bad\syntax_error.nox" >> "%LOG%" 2>&1
echo (exit %ERRORLEVEL%)>> "%LOG%"
echo --- nox bad\runtime_error.nox>> "%LOG%"
"%SRC%\nox.exe" "bad\runtime_error.nox" >> "%LOG%" 2>&1
echo (exit %ERRORLEVEL%)>> "%LOG%"
echo --- noxc -l basics.nox (bytecode listing saved to tests\compiled\listing.txt)>> "%LOG%"
"%SRC%\noxc.exe" -l -p "basics.nox" > "compiled\listing.txt" 2>&1
echo (exit %ERRORLEVEL%)>> "%LOG%"
echo ===== DONE =====>> "%LOG%"
exit /b 0

:one
set "T=%~1"
echo.>> "%LOG%"
echo ===== %T% =====>> "%LOG%"
"%SRC%\nox.exe" "%T%.nox" > "compiled\%T%.source.txt" 2>&1
echo [source run exit %ERRORLEVEL%]>> "%LOG%"
type "compiled\%T%.source.txt" >> "%LOG%"
"%SRC%\noxc.exe" -o "compiled\%T%.out" "%T%.nox" >> "%LOG%" 2>&1
echo [noxc compile exit %ERRORLEVEL%]>> "%LOG%"
"%SRC%\nox.exe" "compiled\%T%.out" > "compiled\%T%.compiled.txt" 2>&1
echo [compiled run exit %ERRORLEVEL%]>> "%LOG%"
fc /b "compiled\%T%.source.txt" "compiled\%T%.compiled.txt" > nul 2>&1 && (echo [compiled output MATCHES source output]>> "%LOG%") || (echo [compiled output DIFFERS - see tests\compiled]>> "%LOG%")
exit /b 0
