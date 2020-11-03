@echo off
rem From visualuefi:
rem -name "VisualUEFI Debugger" -drive file=OVMF_CODE-need-smm.fd,if=pflash,format=raw,unit=0,readonly=on -drive file=OVMF_VARS-need-smm.fd,if=pflash,format=raw,unit=1 -drive file=fat:rw:d:\dev\osdev\joz64\build\,media=disk,if=virtio,format=raw -drive file=UefiShell.iso,format=raw -m 512 -machine q35,smm=on -nodefaults -vga std -global driver=cfi.pflash01,property=secure,value=on -global ICH9-LPC.disable_s3=1

rem using virtio pass through to the file system like this works but it is slow...
rem .\external\qemu.exe -m 512 -L OVMF_dir/ -bios .\external\OVMF-X64-r15214\OVMF.fd -drive file=fat:rw:.\build\,media=disk,if=virtio,format=raw -nodefaults -vga std -global driver=cfi.pflash01,property=secure,value=on -global driver=cfi.pflash01,property=secure,value=on -global ICH9-LPC.disable_s3=1
.\external\qemu.exe -m 512 -smp 2 -L OVMF_dir/ -bios .\external\OVMF-X64-r15214\OVMF.fd -drive format=raw,file=build\boot.dd,if=ide -nodefaults -vga std -global driver=cfi.pflash01,property=secure,value=on -global driver=cfi.pflash01,property=secure,value=on -global ICH9-LPC.disable_s3=1
