@echo off
setlocal EnableDelayedExpansion

set "AVR_GCC=C:\Users\ak199\AppData\Local\Arduino15\packages\arduino\tools\avr-gcc\7.3.0-atmel3.6.1-arduino7\bin\avr-gcc.exe"
set "AVR_OBJCOPY=C:\Users\ak199\AppData\Local\Arduino15\packages\arduino\tools\avr-gcc\7.3.0-atmel3.6.1-arduino7\bin\avr-objcopy.exe"

if not exist DirectBuild mkdir DirectBuild

set "SOURCES="
for %%F in (*.c) do set "SOURCES=!SOURCES! %%F"

"%AVR_GCC%" -std=gnu99 -funsigned-char -funsigned-bitfields -ffunction-sections -fdata-sections -fpack-struct -fshort-enums -Os -Wall -mmcu=atmega16 -I. -Wl,--gc-sections -o DirectBuild\Laptop_Cooling_Pad_Control_System.elf %SOURCES%

if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

"%AVR_OBJCOPY%" -O ihex -R .eeprom -R .fuse -R .lock -R .signature -R .user_signatures DirectBuild\Laptop_Cooling_Pad_Control_System.elf DirectBuild\Laptop_Cooling_Pad_Control_System.hex

if errorlevel 1 (
    echo HEX generation failed.
    exit /b 1
)

echo Build complete: DirectBuild\Laptop_Cooling_Pad_Control_System.hex
