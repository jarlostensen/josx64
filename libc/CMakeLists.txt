target_sources(c PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/string/memcmp.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/string/memcpy.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/string/memmove.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/string/memset.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/string/strlen.c"    
    "${CMAKE_CURRENT_SOURCE_DIR}/internal/_vprint.c"    
    "${CMAKE_CURRENT_SOURCE_DIR}/internal/_wchar.c"    
    "${CMAKE_CURRENT_SOURCE_DIR}/math/pow.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/math/pow_data.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/math/exp_data.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/math/exp2f_data.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/math/ceil.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/math/floor.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/internal/_toupper.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/internal/_tolower.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/internal/_isupper.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/internal/_islower.c"
)
target_include_directories(c PUBLIC 
"${CMAKE_CURRENT_SOURCE_DIR}/include"
"${CMAKE_CURRENT_SOURCE_DIR}/internal/include"
)

#ZZZ: there's a mismash of .a and .lib suffixes being generated and this is probably not really the right way of fixing that....
# can it be set globally?
set_target_properties(c PROPERTIES SUFFIX ".lib")
