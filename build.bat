@echo off
setlocal ENABLEDELAYEDEXPANSION

PATH=%CD%\external\cmake\bin;%CD%\external\LLVM\bin;%CD%\external;%PATH%

external\ninja.exe --verbose -C build

if %ERRORLEVEL% EQU 0 (
    
    rem output yaml version of PDB TODO: debug only
    external\LLVM\bin\llvm-pdbutil.exe pdb2yaml --all build\BOOTX64.PDB > build\BOOTX64.YML

    if not exist %CD%\build\disk mkdir %CD%\build\disk
    if not exist %CD%\build\disk\EFI mkdir %CD%\build\disk\EFI
    if not exist %CD%\build\disk\EFI\BOOT mkdir %CD%\build\disk\EFI\BOOT    
    copy %CD%\build\BOOTX64.EFI %CD%\build\disk\EFI\BOOT
    echo fs0:\EFI\BOOT\BOOTX64.EFI > %CD%\build\disk\startup.nsh

    cd %CD%\build\
    ..\tools\efibootgen.exe -f -d disk -o boot.dd -l "josx64"
    cd ..

    if %ERRORLEVEL% equ 0 (
        del build\josx64_boot.vdi
        VBoxManage.exe convertdd build\boot.dd build\josx64_boot.vdi --format VDI
        rem NOTE: this is just so that I don't have to re-load the vdi in VirtualBox every time it builds
        VBoxManage.exe internalcommands sethduuid build\josx64_boot.vdi cbd893b0-dbcf-483d-a587-6bb45715b677
    )
)

endlocal