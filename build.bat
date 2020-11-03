@echo off
external\ninja.exe -C build

if %ERRORLEVEL% EQU 0 (
    tools\efigen.exe -i build\BOOTX64.EFI -o build\boot.dd -v
)
