@echo off
rem Generate the build.ninja file for all the kernel source using our local ninja.exe and LLVM/clang 10
setlocal ENABLEDELAYEDEXPANSION

PATH=%CD%\external\cmake-3.19.3-win64-x64\bin;%CD%\external\LLVM\bin;%CD%\external;%PATH%

cmake.exe --version

rem https://stackoverflow.com/questions/46553436/building-with-cmake-ninja-and-clang-on-windows
cmake.exe -G Ninja -Bbuild^
 -DCMAKE_C_COMPILER:PATH="%CD%\external\LLVM\bin\clang.exe" -DCMAKE_C_COMPILER_ID="Clang" -DCMAKE_SYSTEM_NAME="Generic"^
 -DCMAKE_LINKER:PATH="%CD%\external\LLVM\bin\lld-link.exe"

endlocal

