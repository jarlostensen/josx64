#!/bin/sh

cmake --version
cmake -H. -Bbuild -DCMAKE_C_COMPILER:PATH="/usr/bin/clang" -DCMAKE_C_COMPILER_ID="Clang" -DCMAKE_SYSTEM_NAME="Generic" -DCMAKE_LINKER:PATH="/usr/bin/ld.lld" -DCMAKE_BUILD_TYPE=Debug
