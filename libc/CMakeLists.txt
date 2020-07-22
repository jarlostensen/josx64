target_sources(c PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/string/memcmp.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/string/memcpy.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/string/memmove.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/string/memset.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/string/strlen.c"
)
target_include_directories(c PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
#ZZZ: there's a mismash of .a and .lib suffixes being generated and this is probably not really the right way of fixing that....
set_target_properties(c PROPERTIES SUFFIX ".lib")
