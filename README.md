# TL;DR josx64
I loved working on the 32-bit from-the-bottom-up jOS kernel project, but having to deal with the arcana of BIOS memory structures and idiosynchrasies got to me in the end, 
and the novelty for a custom built version of GCC also quickly wore off. 
In the end I really wanted to be able to develop and build the kernel entirely on Windows (even though I'll admit WSL is pretty good) and I wanted a more modern interface to machine capabilities 
 and a quicker step into 64-bit land. 
With UEFI I get all that;
* It's modern.
* It loads PE executables straight into 64 bit mode.

# Setup and Tools
All the binaries required to build the project are included and can be downloaded 7zip'd from the releases. The archive must be extracted into a folder called "external" in the root directory of the repo. 
The project uses CMake and Ninja to manage and generate build files and builds using Clang and GCC (both included in the externals folder).

# Project
No particular plan or roadmap exists for this yet. As with jOS it will evolve as I have time to work on it and it is intended for my own personal pleasure and enjoyment. 
If you find it useful then that's great. 
