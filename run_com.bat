@echo off
setlocal
rem === Tunables (real COM mode) ===
set PORT=5555
set OUT=packets.bin
set COM=COM8
set BAUD=115200
rem =================================

set DIR=%~dp0
set BUILD=%DIR%build-com
set RX_EXE=%BUILD%\receiver_cpp\Debug\receiver.exe
set TX_EXE=%BUILD%\sender_c\Debug\sender.exe

if not exist "%RX_EXE%" (
  echo [run_com] receiver not built. Building now...
  call "%DIR%build_com.bat" || exit /b 1
)
if not exist "%TX_EXE%" (
  echo [run_com] sender not built. Building now...
  call "%DIR%build_com.bat" || exit /b 1
)

rem Start receiver
start "receiver (COM)" cmd /k ""%RX_EXE%" --port %PORT% --out %OUT%"

timeout /t 1 >nul

rem Start sender (COM)
start "sender (COM)" cmd /k ""%TX_EXE%"  --com %COM% --baud %BAUD%"

endlocal
