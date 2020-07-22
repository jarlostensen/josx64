#!/bin/bash
cd build
rm fat.img
dd if=/dev/zero of=fat.img bs=1k count=1440
mformat -i fat.img -f 1440 ::
mmd -i fat.img ::/EFI
#mmd -i fat.img ::/EFI/KERNEL
mmd -i fat.img ::/EFI/BOOT
mcopy -i fat.img BOOTX64.EFI ::/EFI/BOOT
#mcopy -i fat.img JosKernel.64 ::/EFI/KERNEL
#mcopy -i fat.img ../../Simple-UEFI-Bootloader/Backend/BOOTX64.EFI ::/EFI/BOOT
mdir -/ -i fat.img
cd ..