@echo off
setlocal
set DIR=%~dp0
set BUILD=%DIR%build-com

if exist "%BUILD%" rmdir /s /q "%BUILD%"
mkdir "%BUILD%"
cd /d "%BUILD%"

rem Configure for REAL COM backend using MSVC
cmake -G "Visual Studio 17 2022" -A x64 -DUSE_EMULATOR=OFF ..

rem Build Debug (or Release)
cmake --build . --config Debug --target receiver sender

echo.
echo ==== Binaries (COM) ====
dir receiver_cpp\Debug\receiver.exe
dir sender_c\Debug\sender.exe
endlocal
