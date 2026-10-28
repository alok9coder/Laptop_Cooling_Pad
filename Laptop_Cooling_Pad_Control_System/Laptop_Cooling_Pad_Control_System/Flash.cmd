@echo off
setlocal

set "AVRDUDE=C:\Users\ak199\AppData\Local\Arduino15\packages\arduino\tools\avrdude\8.0.0-arduino1\bin\avrdude.exe"
set "AVRDUDE_CONF=C:\Users\ak199\AppData\Local\Arduino15\packages\arduino\tools\avrdude\8.0.0-arduino1\etc\avrdude.conf"
set "HEX=DirectBuild\Laptop_Cooling_Pad_Control_System.hex"

if not exist "%AVRDUDE%" (
    echo Avrdude not found: %AVRDUDE%
    exit /b 1
)

if not exist "%AVRDUDE_CONF%" (
    echo Avrdude configuration not found: %AVRDUDE_CONF%
    exit /b 1
)

if not exist "%HEX%" (
    echo HEX file not found: %HEX%
    exit /b 1
)

echo Flashing %HEX% to ATmega16 through USBasp...
"%AVRDUDE%" -C "%AVRDUDE_CONF%" -c usbasp -p m16 -U "flash:w:%HEX%:i"
if errorlevel 1 (
    echo Flash write failed.
    exit /b 1
)

echo Verifying %HEX% against the ATmega16 flash...
"%AVRDUDE%" -C "%AVRDUDE_CONF%" -c usbasp -p m16 -U "flash:v:%HEX%:i"
if errorlevel 1 (
    echo Flash verification failed.
    exit /b 1
)

echo Flash write and verification completed successfully.
endlocal
