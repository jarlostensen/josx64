# TL;DR josx64
I loved working on the 32-bit from-the-bottom-up jOS kernel project but having to deal with the arcana of BIOS memory structures and idiosynchrasies got to me in the end.</br>
I really wanted to be able to develop and build the kernel entirely on Windows (even though I'll admit WSL is pretty good) and inspired by how easy it is to build kernels in Rust I also wanted to use Clang and avoid having to use a specially cross compiled version of gcc.<br/>

Burned by the hassle of old school BIOS I also wanted a more modern interface to machine capabilities, memory layout, etc, so UEFI was the obvious choice: <br/> 
* It provides a rich API for obtaining information about the underlying hardware.
* It loads PE executables straight into 64 bit mode.
<br/>

This project provides those things and is where I'm moving my personal kernel development focus.

<br/>
A huge thanks goes out to the now defunct repo https://github.com/KNNSpeed/Simple-UEFI-Bootloader which really helped get me over the hurdle of even considering my own UEFI loader and getting it building on Windows. Thanks KNNSpeed, whoever you are!<br/>

# Setup and Tools
All the binaries required to build the project are included and can be downloaded 7zip'd from the releases.<br/>
The archive must be extracted into a folder called "external" in the root directory of the repo. <br/>
The project uses CMake and Ninja to manage and generate build files and builds using Clang and GCC (both included in the externals folder).<br/>

To generate the ninja build files run ```genbuild.bat``` in the root folder. This will generate a build folder with all the CMake gubbins and the ninja.build file. To build the project you run ```build.bat``` and to clean it you run ```clean.bat```, that's it.

The output of the build will be a file called ```BOOTX64.EFI``` in the build folder which can be converted to a FAT image using my tool ```efibootgen``` which is included in the release in addition to being available in full source from https://github.com/jarlostensen/efibootgen .
For my own use I've also included a call to VirtualBoxManage in order to generate an image bootable with VBox. You can edit ```build.bat``` to remove or modify that to your needs.

To run the image I use QEMU and ```run_qemu.bat``` has the command line which requires the Tianocore UEFI ROM image included in the external folder.

# Project plans
No particular plan or roadmap exists for this yet. As with jOS it will evolve as I have time to work on it and it is intended for my own personal pleasure and enjoyment.<br/>
If you find it useful then that's great. 

# Tips and sources
I recommend having the standard ready, there's a lot of important information to read there: https://uefi.org/specifications.
OSDEV is as always as invaluable source, for example; https://wiki.osdev.org/UEFI
<br/>
Configuration table GUIDs: https://blog.fpmurphy.com/2015/10/list-efi-configuration-table-entries.html 
