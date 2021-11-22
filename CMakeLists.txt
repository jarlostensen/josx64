#############################################################################
# CMakeLists for josx64 EFI kernel project
# some useful info here https://www.vinnie.work/blog/2020-11-17-cmake-eval/

cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(jOSx64 VERSION 1.0
      DESCRIPTION "64 bit jOS kernel - UEFI"
      LANGUAGES C ASM
)

# include the main cmake recipe for the kernel and the boot code
include(${CMAKE_SOURCE_DIR}/CMakeLists.cmake)

# TESTING: this is where we define main, which is invoked by the kernel after it's started up
add_library(lab STATIC "")
add_subdirectory(lab)
target_link_libraries(boot PRIVATE lab)
