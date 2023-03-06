#
# Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.7.2)

include(settings.cmake)

project(hello C)

find_package(seL4 REQUIRED)
find_package(elfloader-tool REQUIRED)

sel4_import_kernel()

elfloader_import_project()

find_package(musllibc REQUIRED)
find_package(util_libs REQUIRED)
find_package(seL4_libs REQUIRED)

# This sets up environment build flags and imports musllibc and runtime libraries.
musllibc_setup_build_environment_with_sel4runtime()
sel4_import_libsel4()
util_libs_import_libraries()
sel4_libs_import_libraries()

add_executable(hello src/hello.c)

target_link_libraries(hello sel4muslcsys  muslc)

include(rootserver)
DeclareRootserver(hello)

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
