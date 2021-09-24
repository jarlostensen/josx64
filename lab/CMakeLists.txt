# https://cliutils.gitlab.io/modern-cmake/

cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

# overriding options for sub modules
cmake_policy(SET CMP0077 NEW)

set(CMAKE_VERBOSE_MAKEFILE ON)

set(DEBUG_LINKER_FLAGS "-g -debug")
set(DEBUG_BUILD_FLAGS "-Og -g -D_DEBUG")
message("CMAKE system name is \"${CMAKE_SYSTEM_NAME}\", target is \"${BUILD_TARGET}\"")

# https://wiki.osdev.org/UEFI_Bare_Bones#What_to_do_next.3F
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${DEBUG_BUILD_FLAGS} -m64 -mcmodel=small -fshort-wchar -S -fno-exceptions --std=c11 -Wall -Wextra -Wdouble-promotion -Wpedantic")

# NOTE: it appears we need both nostdlib and nodefaultlibs to completly purge the spirit of Windows libriaries from the build
# NOTE: DEBUG 
set(CMAKE_C_LINK_FLAGS "${DEBUG_LINKER_FLAGS} -Wl,-machine:x64 -fuse-ld=lld-link")

set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} --mcmodel=small -mno-stack-arg-probe -m64 -mno-red-zone -maccumulate-outgoing-args -Og -fshort-wchar -fomit-frame-pointer -fno-delete-null-pointer-checks -fno-common -fno-zero-initialized-in-bss -fno-exceptions -fno-stack-protector -fno-stack-check -fno-strict-aliasing -fno-merge-all-constants -fno-merge-constants --std=c11 -g3 -Wall -Wextra -Wdouble-promotion -Wpedantic -fmessage-length=0 -c -MMD -MP")

project(jOSx64Lab VERSION 1.0
        DESCRIPTION "64 bit jOS - Lab"
        LANGUAGES C ASM
        )

enable_language(ASM_NASM)
if(CMAKE_ASM_NASM_COMPILER_LOADED)
  set(CMAKE_ASM_NASM_SOURCE_FILE_EXTENSIONS "asm;nasm;S")
  set(CAN_USE_ASSEMBLER TRUE)

#NOTE: all of the below is needed to force CMake+NASM to output win64 COFF object files.
  set(CMAKE_ASM_NASM_OBJECT_FORMAT win64)  
  set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <INCLUDES> <FLAGS> -f ${CMAKE_ASM_NASM_OBJECT_FORMAT} -o <OBJECT> <SOURCE>")
endif(CMAKE_ASM_NASM_COMPILER_LOADED)

# overide the .a suffix globally
# NOTE: this may need a platform check if we want to build this in a non-Windows environment
set(CMAKE_STATIC_LIBRARY_SUFFIX ".lib")

# the core kernel binary
add_library(kernel STATIC "")
add_subdirectory(kernel)

# joForth library
set(JOFORTH_BUILD_AS_LIB ON CACHE BOOL "")
add_subdirectory("deps/joforth")
target_include_directories(joForth PRIVATE 
"${CMAKE_SOURCE_DIR}/libc/include"
"${CMAKE_SOURCE_DIR}/deps"
"${CMAKE_SOURCE_DIR}/deps/joforth"
)

# Zydis disassembler
# switch off the things we don't want from Zydis/Zycore
set(ZYDIS_BUILD_TOOLS OFF CACHE BOOL "")
set(ZYDIS_BUILD_EXAMPLES OFF CACHE BOOL "")
# this one is important!
set(ZYAN_NO_LIBC ON CACHE BOOL "")
add_subdirectory("deps/zydis")

add_executable(boot "${CMAKE_SOURCE_DIR}/kernel/boot/efi_main.c")
target_include_directories(boot PUBLIC 
"${CMAKE_SOURCE_DIR}/deps/zydis/include" 
"${CMAKE_SOURCE_DIR}/libc/include" 
"${CMAKE_SOURCE_DIR}/libc" 
"${CMAKE_SOURCE_DIR}/c-efi"
"${CMAKE_SOURCE_DIR}/kernel/include"
"${CMAKE_SOURCE_DIR}/deps" 
"${CMAKE_SOURCE_DIR}/kernel"
)

target_link_libraries(boot PRIVATE libc)
target_link_libraries(boot PRIVATE kernel)
target_link_libraries(boot PRIVATE joForth)
target_link_libraries(boot PRIVATE "Zydis")
target_link_libraries(boot PRIVATE "Zycore")
set_target_properties(boot PROPERTIES OUTPUT_NAME "BOOTX64.EFI")
set_target_properties(boot PROPERTIES SUFFIX "")

