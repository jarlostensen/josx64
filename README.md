# TL;DR josx64
I loved working on the 32-bit from-the-bottom-up jOS kernel project but having to deal with the arcana of BIOS memory structures and idiosynchrasies got to me in the end.</br>
I also really wanted was to be able to develop and build the kernel entirely on Windows (even though I'll admit WSL is pretty good) and inspired by how easy it is to build kernels in Rust I also wanted to use Clang and avoid having to use a specially cross compiled version of gcc.<br/>

Burned by the hassle of old school BIOS I also wanted a more modern interface to machine capabilities, memory layout, etc, so UEFI was the obvious choice: <br/> 
* It provides a rich API for obtaining information about the underlying hardware.
* It loads PE executables straight into 64 bit mode.
<br/>

This project provides those things and is where I'm moving my personal kernel development focus.

<br/>
A huge thanks goes out to the now defunct repo https://github.com/KNNSpeed/Simple-UEFI-Bootloader which really helped get me over the hurdle of even considering my own UEFI loader and getting it building on Windows. Thanks KNNSpeed, whoever you are!<br/>
<br/>

# Setup and Tools
The project is built using clang on Windows, with Ninja as the build system. This choice has presented a few challenges and therefore the build system as it stands is *not* generally usable (i.e. don't expect to be able to clone and build anything yet).

I have also had to create my own boot image tool (again because I'm doing all this on Windows) which can be found here: https://github.com/jarlostensen/efibootgen and I'm also developing a debugger for the project to make my life easier (and because it is fun): https://github.com/jarlostensen/joKDbg .

# Project plans
No particular plan or roadmap exists for this yet. As with jOS it will evolve as I have time to work on it and it is intended for my own personal pleasure and enjoyment.<br/>
If you find it useful then that's great. 

# Tips and sources
I recommend having the standard ready, there's a lot of important information to read there: https://uefi.org/specifications.
OSDEV is as always as invaluable source, for example; https://wiki.osdev.org/UEFI
<br/>
Configuration table GUIDs: https://blog.fpmurphy.com/2015/10/list-efi-configuration-table-entries.html 
