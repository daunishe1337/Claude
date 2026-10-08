@echo off
rem Сборка без CMake: запустите из "Developer Command Prompt for Visual Studio"
cl /nologo /O2 /EHsc /std:c++17 /utf-8 /MT /DNDEBUG /D_CRT_SECURE_NO_WARNINGS src\*.cpp /Fe:PCSimulator2.exe ^
   /link opengl32.lib gdi32.lib user32.lib winmm.lib /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup
if errorlevel 1 (
  echo Сборка не удалась
  exit /b 1
)
del /q *.obj 2>nul
echo Готово: PCSimulator2.exe
