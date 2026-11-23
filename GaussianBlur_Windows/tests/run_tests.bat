@echo off
rem Запускать из "x64 Native Tools Command Prompt for VS". Photoshop и SDK не нужны.
cd /d "%~dp0\.."
cl /nologo /std:c++17 /EHsc /W3 tests\test_core.cpp src\GaussianBlurCore.cpp /Fe:tests\test_core.exe /Fo:tests\ || exit /b 1
tests\test_core.exe
exit /b %errorlevel%
