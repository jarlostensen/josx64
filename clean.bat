@echo off
external\ninja.exe -C build clean
del /Q build\*.vdi
del /Q build\*.dd
del /Q /S build\EFI
