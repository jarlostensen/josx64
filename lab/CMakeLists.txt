target_include_directories(lab PRIVATE
    "${CMAKE_SOURCE_DIR}/libc/include"
    "${CMAKE_SOURCE_DIR}/kernel/include"
    "${CMAKE_SOURCE_DIR}/deps"
)

target_sources(lab PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/hello_world.c"    
)

#ZZZ: there's a mismash of .a and .lib suffixes being generated and this is probably not really the right way of fixing that....
# can it be set globally?
set_target_properties(kernel PROPERTIES SUFFIX ".lib")

