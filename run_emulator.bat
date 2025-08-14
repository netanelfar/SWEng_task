@echo off
setlocal
rem === Tunables (emulator mode) ===
set PORT=5555
set OUT=packets.bin
set BAUD=28800
rem ================================

set DIR=%~dp0
set BUILD=%DIR%build-emul
set RX_EXE=%BUILD%\receiver_cpp\Debug\receiver.exe
set TX_EXE=%BUILD%\sender_c\Debug\sender.exe

if not exist "%RX_EXE%" (
  echo [run_emulator] receiver not built. Building now...
  call "%DIR%build_emulator.bat" || exit /b 1
)
if not exist "%TX_EXE%" (
  echo [run_emulator] sender not built. Building now...
  call "%DIR%build_emulator.bat" || exit /b 1
)

rem Start receiver
start "receiver (emulator)" cmd /k ""%RX_EXE%" 
rem Give receiver a moment to bind
timeout /t 1 >nul

rem Start sender (emulator)
start "sender (emulator)" cmd /k ""%TX_EXE%" 

endlocal
