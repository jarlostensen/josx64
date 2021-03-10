@echo off
setlocal ENABLEDELAYEDEXPANSION

external\ninja.exe -C build

if %ERRORLEVEL% EQU 0 (
    
    rem output yaml version of PDB TODO: debug only
    external\LLVM\bin\llvm-pdbutil.exe pdb2yaml --all build\BOOTX64.PDB > build\BOOTX64.YML

    if not exist %CD%\build\EFI mkdir %CD%\build\EFI
    if not exist %CD%\build\EFI\BOOT mkdir %CD%\build\EFI\BOOT
    if not exist %CD%\build\EFI\ASSETS mkdir %CD%\build\EFI\ASSETS
    copy %CD%\build\BOOTX64.PDB %CD%\build\EFI\ASSETS
    copy %CD%\build\BOOTX64.YML %CD%\build\EFI\ASSETS
    copy %CD%\build\BOOTX64.EFI %CD%\build\EFI\BOOT

    rem tools\efibootgen.exe -v -b build\BOOTX64.EFI -o build\boot.dd -l "josx64"
    cd %CD%\build\
    ..\tools\efibootgen.exe -f -d EFI -o boot.dd -l "josx64"
    cd ..

    if %ERRORLEVEL% equ 0 (
        del build\josx64_boot.vdi
        "c:\Program Files\Oracle\VirtualBox\VBoxManage.exe" convertdd build\boot.dd build\josx64_boot.vdi --format VDI
        rem NOTE: this is just so that I don't have to re-load the vdi in VirtualBox every time it builds
        "c:\Program Files\Oracle\VirtualBox\VBoxManage.exe" internalcommands sethduuid build\josx64_boot.vdi cbd893b0-dbcf-483d-a587-6bb45715b677
    )
)

endlocal