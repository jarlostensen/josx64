#!/bin/sh

cd build
qemu-system-x86_64 -m 4G -smp 2 -L OVMF_dir/ -bios /usr/share/ovmf/OVMF.fd -drive format=raw,file=josxhd.bin,if=ide -vga std -display sdl -serial file:qemu_serial.txt
cd ..
