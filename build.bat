@echo off
rem ===========================================================================
rem  Сборка HackTerm. Использование:
rem    build.bat          - автоматически: MSVC (если cl доступен), иначе g++
rem    build.bat msvc     - Visual Studio (запускать из "Developer Command Prompt")
rem    build.bat gcc      - MinGW-w64 g++
rem ===========================================================================
setlocal
set MODE=%1
if "%MODE%"=="" (
    where cl >nul 2>nul && (set MODE=msvc) || (set MODE=gcc)
)

if /I "%MODE%"=="msvc" (
    cl /nologo /std:c++17 /EHsc /O2 /W4 /utf-8 /DUNICODE /D_UNICODE src\*.cpp /Fe:hackterm.exe /link user32.lib
    if errorlevel 1 goto :fail
    del /q *.obj >nul 2>nul
    goto :ok
)
if /I "%MODE%"=="gcc" (
    g++ -std=c++17 -O2 -Wall -Wextra -static -o hackterm.exe src\*.cpp
    if errorlevel 1 goto :fail
    goto :ok
)
echo Unknown mode: %MODE%
exit /b 1

:ok
echo.
echo [+] hackterm.exe built successfully
exit /b 0

:fail
echo.
echo [-] Build failed
exit /b 1
