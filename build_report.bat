@echo off
rem ===========================================================================
rem  build_report.bat -- compile and run the sort/report module self test
rem
rem  Usage: double-click this file, or run it from a command prompt.
rem  Note : messages are ASCII on purpose, so they never turn into mojibake
rem         in a console whose code page is GBK or UTF-8.
rem ===========================================================================
setlocal

where cl >nul 2>&1
if not errorlevel 1 goto :build

rem ---- no cl on PATH: locate Visual Studio and load its build environment ----
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :nocompiler

"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%TEMP%\vs_install_path.txt" 2>nul
set "VSPATH="
set /p VSPATH=<"%TEMP%\vs_install_path.txt"
del "%TEMP%\vs_install_path.txt" >nul 2>&1

if not defined VSPATH goto :nocompiler
if not exist "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" goto :nocompiler

echo [INFO] Visual Studio found: %VSPATH%
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

:build
echo [INFO] Compiling record.cpp + sort_report.cpp + sort_report_test.cpp ...
cl /nologo /EHsc /utf-8 /W4 record.cpp sort_report.cpp sort_report_test.cpp /Fe:sort_report_test.exe
if errorlevel 1 (
    echo.
    echo [ERROR] Build failed, please check the messages above.
    pause
    exit /b 1
)

chcp 65001 >nul
echo.
echo [INFO] Build OK, running the self test and saving the report sample ...
echo ------------------------------------------------------------
sort_report_test.exe > "乙_报表样例输出.txt"
type "乙_报表样例输出.txt"
echo ------------------------------------------------------------
echo [INFO] Sample output saved to 乙_报表样例输出.txt
echo [INFO] Done. Press any key to close this window.
pause >nul
exit /b 0

:nocompiler
echo [ERROR] C++ compiler not found.
echo         Run this script in "x64 Native Tools Command Prompt for VS 2022",
echo         or install the "Desktop development with C++" workload first.
pause
exit /b 1
