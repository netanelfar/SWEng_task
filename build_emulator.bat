@echo off
setlocal
set DIR=%~dp0
set BUILD=%DIR%build-emul

if exist "%BUILD%" rmdir /s /q "%BUILD%"
mkdir "%BUILD%"
cd /d "%BUILD%"

rem Configure for EMULATOR backend using MSVC
cmake -G "Visual Studio 17 2022" -A x64 -DUSE_EMULATOR=ON ..

rem Build Debug (or Release if you prefer)
cmake --build . --config Debug --target receiver sender

echo.
echo ==== Binaries (EMULATOR) ====
dir receiver_cpp\Debug\receiver.exe
dir sender_c\Debug\sender.exe
endlocal
