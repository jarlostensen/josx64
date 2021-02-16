cmake_minimum_required(VERSION 3.18)
project(joForth)
set(CMAKE_C_STANDARD 11)

option(JOFORTH_BUILD_AS_LIB "build as library" OFF)

if(JOFORTH_BUILD_AS_LIB)
add_library(joForth joforth.c jo.h joforth_ir.h)
else()
add_executable(joForth joforth.c main.c "jo.h" "joforth_ir.h")
endif()

target_include_directories(joForth PRIVATE 
    "${CMAKE_SOURCE_DIR}"    
)