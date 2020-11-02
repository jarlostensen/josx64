# https://cliutils.gitlab.io/modern-cmake/

cmake_minimum_required(VERSION 3.12)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

# https://wiki.osdev.org/UEFI_Bare_Bones#What_to_do_next.3F
set(CMAKE_C_FLAGS 
"${CMAKE_C_FLAGS} -D_JOS_KERNEL_BUILD -target x86_64-unknown-windows -isystem="${CMAKE_SOURCE_DIR}/sysroot/include" -DGNU_EFI_USE_MS_ABI -g -m64 -nostdinc -ffreestanding -mcmodel=small -fshort-wchar -mno-red-zone -fno-builtin-setjmp -fno-builtin-longjmp -fomit-frame-pointer -fno-delete-null-pointer-checks -fno-common -fno-zero-initialized-in-bss -fno-exceptions -fno-stack-protector -fno-stack-check -fno-strict-aliasing -fno-merge-all-constants --std=c11 -Wall -Wextra -Wdouble-promotion -Wpedantic ")

# NOTE: it appears we need both nodefaultlibs and nodefaultlibs to completly purge the spirit of Windows libriaries from the build
set(CMAKE_C_LINK_FLAGS 
"-target x86_64-unknown-windows -nostdlib -nodefaultlibs -Wl,-machine:x64 -Wl,-entry:efi_main -Wl,-subsystem:efi_application -fuse-ld=lld-link")

set(CMAKE_ASM_FLAGS
"${CMAKE_ASM_FLAGS} -D_JOS_KERNEL_BUILD -DGNU_EFI_USE_MS_ABI -mno-avx -mcmodel=small -mno-stack-arg-probe -m64 -mno-red-zone -maccumulate-outgoing-args -Og -ffreestanding -fshort-wchar -fomit-frame-pointer -fno-delete-null-pointer-checks -fno-common -fno-zero-initialized-in-bss -fno-exceptions -fno-stack-protector -fno-stack-check -fno-strict-aliasing -fno-merge-all-constants -fno-merge-constants --std=c11 -g3 -Wall -Wextra -Wdouble-promotion -Wpedantic -fmessage-length=0 -c -MMD -MP"
)

project(jOSx64 VERSION 1.0
        DESCRIPTION "64 bit jOS kernel - UEFI"
        LANGUAGES C ASM
        )

set(CMAKE_CROSSCOMPILING TRUE)
SET(CMAKE_SYSTEM_NAME Generic)

# https://metricpanda.com/rival-fortress-update-27-compiling-with-clang-on-windows/
# https://stackoverflow.com/questions/28597351/how-do-i-add-a-library-path-in-cmake
# https://stackoverflow.com/questions/34709286/cmake-configure-deep-sub-folder-tree

add_library(c STATIC "")
add_subdirectory(libc)

add_library(kernel STATIC "")
add_subdirectory(kernel)

add_executable(boot "${CMAKE_SOURCE_DIR}/kernel/boot/efi_main.c")
target_include_directories(boot PUBLIC 
"${CMAKE_SOURCE_DIR}/libc/include" 
"${CMAKE_SOURCE_DIR}/libc" 
"${CMAKE_SOURCE_DIR}/c-efi"
"${CMAKE_SOURCE_DIR}/kernel/include"
)

target_link_libraries(boot PRIVATE libc)
target_link_libraries(boot PRIVATE kernel)
set_target_properties(boot PROPERTIES OUTPUT_NAME "BOOTX64.EFI")
set_target_properties(boot PROPERTIES SUFFIX "")

