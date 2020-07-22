# https://cliutils.gitlab.io/modern-cmake/

cmake_minimum_required(VERSION 3.12)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

# https://wiki.osdev.org/UEFI_Bare_Bones#What_to_do_next.3F
#"${CMAKE_C_FLAGS} -D_JOS_KERNEL_BUILD -target x86_64-unknown-windows --sysroot="${CMAKE_SOURCE_DIR}/sysroot" -isystem="${CMAKE_SOURCE_DIR}/sysroot/include" -DGNU_EFI_USE_MS_ABI -nostdinc -m64 -Og -mno-avx -mcmodel=small -mno-stack-arg-probe -ffreestanding -fshort-wchar -mno-red-zone  -fomit-frame-pointer -fno-delete-null-pointer-checks -fno-common -fno-zero-initialized-in-bss -fno-exceptions -fno-stack-protector -fno-stack-check -fno-strict-aliasing -fno-merge-all-constants --std=c11  -g3 -Wall -Wextra -Wdouble-promotion -Wpedantic -fmessage-length=0 -c -Wno-unknown-pragmas")
set(CMAKE_C_FLAGS 
"${CMAKE_C_FLAGS} -D_JOS_KERNEL_BUILD -target x86_64-unknown-windows -isystem="${CMAKE_SOURCE_DIR}/sysroot/include" -DGNU_EFI_USE_MS_ABI -g -m64 -nostdinc -ffreestanding -mcmodel=small -fshort-wchar -mno-red-zone -fno-builtin-setjmp -fno-builtin-longjmp -fomit-frame-pointer -fno-delete-null-pointer-checks -fno-common -fno-zero-initialized-in-bss -fno-exceptions -fno-stack-protector -fno-stack-check -fno-strict-aliasing -fno-merge-all-constants --std=c11 -Wall -Wextra -Wdouble-promotion -Wpedantic ")

set(CMAKE_C_LINK_FLAGS 
"-target x86_64-unknown-windows -nostdlib -Wl,-machine:x64 -Wl,-entry:efi_main -Wl,-subsystem:efi_application -fuse-ld=lld-link")

set(CMAKE_ASM_FLAGS
"${CMAKE_ASM_FLAGS} -D_JOS_KERNEL_BUILD -DGNU_EFI_USE_MS_ABI -mno-avx -mcmodel=small -mno-stack-arg-probe -m64 -mno-red-zone -maccumulate-outgoing-args -Og -ffreestanding -fshort-wchar -fomit-frame-pointer -fno-delete-null-pointer-checks -fno-common -fno-zero-initialized-in-bss -fno-exceptions -fno-stack-protector -fno-stack-check -fno-strict-aliasing -fno-merge-all-constants -fno-merge-constants --std=c11 -g3 -Wall -Wextra -Wdouble-promotion -Wpedantic -fmessage-length=0 -c -MMD -MP"
)

project(jOSx64 VERSION 1.0
        DESCRIPTION "64 bit jOS kernel - UEFI"
        LANGUAGES C ASM
        )

set(CMAKE_CROSSCOMPILING TRUE)
SET(CMAKE_SYSTEM_NAME Generic)

# set(CMAKE_C_COMPILER "${CMAKE_SOURCE_DIR}external/LLVM/bin/clang.exe")
# https://metricpanda.com/rival-fortress-update-27-compiling-with-clang-on-windows/

# https://stackoverflow.com/questions/34709286/cmake-configure-deep-sub-folder-tree
add_library(c STATIC "")
add_subdirectory(libc)

# TODO: add_library(kernel STATIC "")
# TODO: add_subdirectory(kernel)

add_library(gnu-efi "")
add_subdirectory(gnu-efi-3.0.9)

add_executable(boot "${CMAKE_SOURCE_DIR}/kernel/boot/efi_main.c")
target_include_directories(boot PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/libc/include" "${CMAKE_CURRENT_SOURCE_DIR}/gnu-efi-3.0.9/inc" "${CMAKE_CURRENT_SOURCE_DIR}/gnu-efi-3.0.9/inc/x86_64" "${CMAKE_CURRENT_SOURCE_DIR}/gnu-efi-3.0.9/inc/protocol")
target_link_libraries(boot PRIVATE libc gnu-efi)
set_target_properties(boot PROPERTIES OUTPUT_NAME "BOOTX64.EFI")
set_target_properties(boot PROPERTIES SUFFIX "")

