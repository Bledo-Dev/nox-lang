@echo off
rem Builds Nox with w64devkit, then runs the test scripts. Output goes to build_log.txt
setlocal
set "PATH=C:\LocalCompile\Compilers\w64devkit\bin;%PATH%"
set "LOG=%~dp0build_log.txt"
cd /d "%~dp0src" || (echo src folder not found> "%LOG%" & exit /b 1)
echo === make clean ===> "%LOG%"
make clean >> "%LOG%" 2>&1
echo === make mingw ===>> "%LOG%"
make mingw >> "%LOG%" 2>&1
set "RC=%ERRORLEVEL%"
>> "%LOG%" echo BUILD EXIT CODE %RC%
if not "%RC%"=="0" goto done
if not exist nox.exe goto done
echo === nox -v ===>> "%LOG%"
nox.exe -v >> "%LOG%" 2>&1
for %%T in (json_test sandbox_test web_test) do (echo === %%T ===>> "%LOG%" & nox.exe %%T.nox >> "%LOG%" 2>&1)
:done
echo === DONE ===>> "%LOG%"
