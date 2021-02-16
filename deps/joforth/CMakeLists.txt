cmake_minimum_required(VERSION 3.18)
project(joForth)
set(CMAKE_C_STANDARD 11)

option(JOFORTH_BUILD_AS_LIB "build as library" OFF)

if(JOFORTH_BUILD_AS_LIB)
message("${PROJECT_NAME}: building as library")
add_library(${PROJECT_NAME} STATIC "${CMAKE_CURRENT_SOURCE_DIR}/joforth.c")
else()
message("${PROJECT_NAME}: building executable")
add_executable(${PROJECT_NAME} joforth.c main.c "jo.h" "joforth_ir.h")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}"    
    "${CMAKE_CURRENT_SOURCE_DIR}/deps"
)