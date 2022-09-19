#!/bin/sh

cd build/
dd if=/dev/zero of=fat.img bs=1k count=1440
mformat -i fat.img -f 1440 ::
mmd -i fat.img ::/EFI
mmd -i fat.img ::/EFI/BOOT
mcopy -i fat.img BOOTX64.EFI ::/EFI/BOOT

mkgpt -o josxhd.bin --image-size 4096 --part fat.img --type system
rm fat.img

cd ..
