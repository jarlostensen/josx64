#############################################################################
# CMakeLists for josx64 EFI kernel project

cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

# overriding options for sub modules
cmake_policy(SET CMP0077 NEW)
set(CMAKE_VERBOSE_MAKEFILE ON)

project(jOSx64 VERSION 1.0
      DESCRIPTION "64 bit jOS kernel - UEFI"
      LANGUAGES C ASM
)

#########################################################################
# misc defines

set(BUILD_TARGET "x86_64-unknown-windows")
set(OPT_FLAGS -Og)
set(DEBUG_FLAGS -g -D_DEBUG)
set(COMPILER_CODEGEN_FLAGS -m64 -mcmodel=large -fshort-wchar -mno-red-zone -fno-builtin-setjmp -fno-builtin-longjmp -fomit-frame-pointer -fno-delete-null-pointer-checks -fno-common -fno-zero-initialized-in-bss -fno-exceptions -fno-stack-protector -fno-stack-check -fno-strict-aliasing -fno-merge-all-constants --std=c11 -Wall)
set(COMPILER_FLAGS -D_JOS_KERNEL_BUILD -D_JO_BARE_METAL_BUILD "${COMPILER_CODEGEN_FLAGS}" -target "${BUILD_TARGET}" -nostdinc -ffreestanding -isystem="${CMAKE_SOURCE_DIR}/libc/include" "${OPT_FLAGS}" "${DEBUG_FLAGS}")
set(LINKER_FLAGS 
    LINKER:-nodefaultlib,
    LINKER:-subsystem:efi_application,
    LINKER:-entry:efi_main,
    LINKER:-debug,
    LINKER:-pdb:BOOTX64.PDB
)

# set the compiler flags for this directory and all descendants, i.e. the libs we're including
add_compile_options("${COMPILER_FLAGS}")

message("CMAKE system name is \"${CMAKE_SYSTEM_NAME}\", target is \"${BUILD_TARGET}\"")

#############################################################################
# NASM 

enable_language(ASM_NASM)
if(CMAKE_ASM_NASM_COMPILER_LOADED)
  set(CMAKE_ASM_NASM_SOURCE_FILE_EXTENSIONS "asm;nasm;S")
  set(CAN_USE_ASSEMBLER TRUE)
#NOTE: all of the below is needed to force CMake+NASM to output win64 COFF object files.
  set(CMAKE_ASM_NASM_OBJECT_FORMAT win64)  
  set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> -f ${CMAKE_ASM_NASM_OBJECT_FORMAT} -o <OBJECT> <SOURCE>")
endif(CMAKE_ASM_NASM_COMPILER_LOADED)

set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PREFIX_PATH  "${CMAKE_SOURCE_DIR}/libc/include")

# https://metricpanda.com/rival-fortress-update-27-compiling-with-clang-on-windows/
# https://stackoverflow.com/questions/28597351/how-do-i-add-a-library-path-in-cmake
# https://stackoverflow.com/questions/34709286/cmake-configure-deep-sub-folder-tree

# overide the .a suffix globally
# NOTE: this may need a platform check if we want to build this in a non-Windows environment
set(CMAKE_STATIC_LIBRARY_SUFFIX ".lib")

# libc, including bits of MUSL
add_library(c STATIC "")
add_subdirectory(libc)

# the core kernel binary
add_library(kernel STATIC "")
add_subdirectory(kernel)

# joForth library
# set(JOFORTH_BUILD_AS_LIB ON CACHE BOOL "")
# add_subdirectory("deps/joforth")
# target_include_directories(joForth PRIVATE 
# "${CMAKE_SOURCE_DIR}/libc/include"
# "${CMAKE_SOURCE_DIR}/deps"
# "${CMAKE_SOURCE_DIR}/deps/joforth"
# )

# Zydis disassembler
# switch off the things we don't want from Zydis/Zycore
set(ZYDIS_BUILD_TOOLS OFF CACHE BOOL "")
set(ZYDIS_BUILD_EXAMPLES OFF CACHE BOOL "")
# this one is important!
set(ZYAN_NO_LIBC ON CACHE BOOL "")
add_subdirectory("deps/zydis")

add_executable(boot "${CMAKE_SOURCE_DIR}/kernel/boot/efi_main.c")
target_include_directories(boot PRIVATE 
"${CMAKE_SOURCE_DIR}/deps/zydis/include" 
"${CMAKE_SYSTEM_PREFIX_PATH}" 
"${CMAKE_SOURCE_DIR}/libc" 
"${CMAKE_SOURCE_DIR}/c-efi"
"${CMAKE_SOURCE_DIR}/kernel/include"
"${CMAKE_SOURCE_DIR}/deps" 
"${CMAKE_SOURCE_DIR}/kernel"
)

target_link_libraries(boot PRIVATE libc)
target_link_libraries(boot PRIVATE kernel)
# target_link_libraries(boot PRIVATE joForth)
target_link_libraries(boot PRIVATE "Zydis")
target_link_libraries(boot PRIVATE "Zycore")

target_link_options(boot PRIVATE "${LINKER_FLAGS}")

set_target_properties(boot PROPERTIES SUFFIX "")
set_target_properties(boot PROPERTIES OUTPUT_NAME "BOOTX64.EFI")


