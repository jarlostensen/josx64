target_sources(gnu-efi PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/misc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/print.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/smbios.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/sread.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/str.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/boxdraw.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/cmdline.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/console.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/crc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/data.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/debug.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/dpath.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/error.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/event.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/exit.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/guid.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/hand.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/hw.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/init.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/lock.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/x86_64/callwrap.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/x86_64/initplat.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/x86_64/math.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/runtime/efirtlib.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/runtime/rtdata.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/runtime/rtlock.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/runtime/rtstr.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/runtime/vm.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/x86_64/efi_stub.S"
    "${CMAKE_CURRENT_SOURCE_DIR}/lib/x86_64/setjmp.S"
)

set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/lib/x86_64/setjmp.S" PROPERTY LANGUAGE ASM)
set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/lib/x86_64/efi_stub.S" PROPERTY LANGUAGE ASM)

target_include_directories(gnu-efi PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/inc" "${CMAKE_CURRENT_SOURCE_DIR}/inc/x86_64" "${CMAKE_CURRENT_SOURCE_DIR}/inc/protocol" "${CMAKE_CURRENT_SOURCE_DIR}/lib")
#ZZZ: there's a mismash of .a and .lib suffixes being generated and this is probably not really the right way of fixing that....
set_target_properties(gnu-efi PROPERTIES SUFFIX ".lib")
