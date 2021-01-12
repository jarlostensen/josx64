@echo off
external\ninja.exe -C build

if %ERRORLEVEL% EQU 0 (
    tools\efibootgen.exe -v -b build\BOOTX64.EFI -o build\boot.dd -l "josx64"

    if %ERRORLEVEL% equ 0 (
        del build\josx64_boot.vdi
        "c:\Program Files\Oracle\VirtualBox\VBoxManage.exe" convertdd build\boot.dd build\josx64_boot.vdi --format VDI
        rem NOTE: this is just so that I don't have to re-load the vdi in VirtualBox every time it builds
        "c:\Program Files\Oracle\VirtualBox\VBoxManage.exe" internalcommands sethduuid build\josx64_boot.vdi cbd893b0-dbcf-483d-a587-6bb45715b677
    )
)
