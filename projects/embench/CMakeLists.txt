#
# Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.7.2)

include(settings.cmake)

project(embench C ASM)

# Kernel configuration variables
# sets EXPORT_PMU_USER: true which enables PMU usage on userspace. This allows
# the usage of performance counters
set(KernelArmExportPMUUser ON CACHE BOOL "" FORCE) 

find_package(seL4 REQUIRED)
find_package(elfloader-tool REQUIRED)
find_package(musllibc REQUIRED)
find_package(util_libs REQUIRED)
find_package(seL4_libs REQUIRED)
find_package(attester_sw REQUIRED)

sel4_import_kernel()
elfloader_import_project()

# This sets up environment build flags and imports musllibc and runtime libraries.
musllibc_setup_build_environment_with_sel4runtime()
sel4_import_libsel4()
util_libs_import_libraries()
sel4_libs_import_libraries()
attester_sw_import_library()

add_executable(app support/app.c)

# Include the pass in the app elf file
#target_compile_options(app PRIVATE -fpass-plugin=/home/mskordal/workspace/myRepos/my-sel4-projects/llvmpassbuild/libInjectBRAM.so)

target_link_libraries(app
	sel4 muslc sel4muslcsys
	sel4utils)


include(cpio)
MakeCPIO(archive.o "$<TARGET_FILE:app>")


file(GLOB CSOURCES src/*/*.c support/*.c)
file(GLOB CHEADERS src/*/*.h support/*.h)

# remove app.c from list since it is separately added through archive.o
list(REMOVE_ITEM CSOURCES "${CMAKE_CURRENT_SOURCE_DIR}/support/app.c")

add_executable(embench archive.o ${CSOURCES} ${CHEADERS})

target_include_directories(
	embench
	PRIVATE
		support
)

# Include the pass in the main elf file
target_compile_options(embench PRIVATE -O0 -fpass-plugin=/home/mskordal/workspace/myRepos/my-sel4-projects/llvmpassbuild/libInjectBRAM.so)

target_link_libraries(embench 
	sel4
    muslc
    utils
    sel4muslcsys
	sel4platsupport
    sel4utils
    sel4debug
	sel4allocman
    sel4bench
	attester_sw
)


include(rootserver)
DeclareRootserver(embench)

if(SIMULATION)
    include(simulation)
    if(KernelSel4ArchX86_64)
        SetSimulationScriptProperty(MEM_SIZE "3G")
    endif()
    if(KernelPlatformQEMUArmVirt)
        SetSimulationScriptProperty(MEM_SIZE "2G")
    endif()
    GenerateSimulateScript()
endif()
