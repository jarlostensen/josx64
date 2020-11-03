target_include_directories(kernel PUBLIC 
    "${CMAKE_SOURCE_DIR}/c-efi"
    "${CMAKE_SOURCE_DIR}/libc/include"
    "${CMAKE_SOURCE_DIR}/kernel/include"
    "${CMAKE_SOURCE_DIR}/kernel/include"    
)

target_sources(kernel PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/video.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/memory.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/output_console.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/serial.c"
)

#ZZZ: there's a mismash of .a and .lib suffixes being generated and this is probably not really the right way of fixing that....
# can it be set globally?
set_target_properties(kernel PROPERTIES SUFFIX ".lib")

