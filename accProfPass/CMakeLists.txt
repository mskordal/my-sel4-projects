cmake_minimum_required(VERSION 3.13.4)
project(accprof-access-instructions)

#===============================================================================
# 1. LOAD LLVM CONFIGURATION
#===============================================================================
# Set this to a valid LLVM installation dir
set(LT_LLVM_INSTALL_DIR "" CACHE PATH "LLVM installation directory")

set(USE_CLANG OFF CACHE BOOL "Pass define to build for clang or opt" ) 
set(USE_HLS OFF CACHE BOOL "Pass define to build for HLS or AccProf-soft" ) 
set(DEBUG_COMP_PRINT OFF CACHE BOOL "Pass define to print debug messages during compilation" ) 

# Add the location of LLVMConfig.cmake to CMake search paths (so that
# find_package can locate it)
list(APPEND CMAKE_PREFIX_PATH "${LT_LLVM_INSTALL_DIR}/lib/cmake/llvm/")

# FIXME: This is a warkaround for #25. Remove once resolved and use
find_package(LLVM 15 REQUIRED CONFIG)

# AccProf includes headers from LLVM - update the include paths accordingly
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})

#===============================================================================
# 2. LLVM-TUTOR BUILD CONFIGURATION
#===============================================================================
# Use the same C++ standard as LLVM does
set(CMAKE_CXX_STANDARD 17 CACHE STRING "")

# LLVM is normally built without RTTI. Be consistent with that.
if(NOT LLVM_ENABLE_RTTI)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

#===============================================================================
# 4. ADD DEFINITIONS
#===============================================================================
if (USE_CLANG)
  add_definitions(-DUSE_CLANG)
endif()
if (USE_HLS)
  add_definitions(-DUSE_HLS)
endif()
if (DEBUG_COMP_PRINT)
  add_definitions(-DDEBUG_COMP_PRINT)
endif()

#===============================================================================
# 3. ADD THE TARGET
#===============================================================================
add_library(AccProf SHARED AccProf.cpp)

# target_compile_options( AccProf PRIVATE ${USE_CLANG} ${USE_HLS} )
# Allow undefined symbols in shared objects on Darwin (this is the default
# behaviour on Linux)
target_link_libraries(AccProf
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")
