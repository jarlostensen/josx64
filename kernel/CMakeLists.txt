target_include_directories(kernel PRIVATE
    "${CMAKE_SOURCE_DIR}/c-efi"
    "${CMAKE_SOURCE_DIR}/libc/include"
    "${CMAKE_SOURCE_DIR}/kernel/programs"
    "${CMAKE_SOURCE_DIR}/kernel/include"
    "${CMAKE_SOURCE_DIR}/deps"
#JO: NOTE: the build requires Zy[dis|core]ExportConfig files which are generated by CMake for Zydis and Zycore. 
#    TODO: have them generated by clang (instead of VS) so that they use the correct attributes and we don't have to hand edit them
    "${CMAKE_SOURCE_DIR}/deps/zydis" 
    "${CMAKE_SOURCE_DIR}/deps/zydis/include" 
    "${CMAKE_SOURCE_DIR}/deps/zydis/dependencies/zycore" 
    "${CMAKE_SOURCE_DIR}/deps/zydis/dependencies/zycore/include" 
)

target_sources(kernel PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/video.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/memory.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/output_console.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/serial.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/smp.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/interrupts.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/apic.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/kernel.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/clock.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/debugger.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/hex_dump.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/keyboard.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/trace.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/pe.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/i8259a.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/i8253.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/tasks.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/x86_64.asm"
    "${CMAKE_CURRENT_SOURCE_DIR}/programs/scroller.c"
)

#ZZZ: there's a mismash of .a and .lib suffixes being generated and this is probably not really the right way of fixing that....
# can it be set globally?
set_target_properties(kernel PROPERTIES SUFFIX ".lib")

