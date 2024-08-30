#
# Copyright 2019, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

cmake_minimum_required(VERSION 3.7.2)

include(settings.cmake)

project(stream C ASM)

# Kernel configuration variables
# sets EXPORT_PMU_USER: true which enables PMU usage on userspace. This allows
# the usage of performance counters
set(KernelArmExportPMUUser ON CACHE BOOL "" FORCE) 

set(LLVMPass "" CACHE FILEPATH "The LLVM pass to instrument the app")
set(FunctionsFile "" CACHE FILEPATH "The text file with the functions the pass takes")
set(EventsFile "" CACHE FILEPATH "The text file with the events the pass takes")
set(ProfMetaFile "" CACHE FILEPATH "The text file the pass takes that contains profiling metadata. Only used at profiling pass")
set(EventShiftsFile "" CACHE FILEPATH "The text file the pass takes that shows how many bits to shift events for each function. Only used at attestation pass")
set(KeysFile "" CACHE FILEPATH "The text file with the keys the pass takes. Only used at attestation pass")

set(ProfMetaFlag "")
set(EventShiftsFlag "")
set(KeysFlag "")

find_package(seL4 REQUIRED)
find_package(elfloader-tool REQUIRED)
find_package(musllibc REQUIRED)
find_package(util_libs REQUIRED)
find_package(seL4_libs REQUIRED)
find_package(accprof_soft REQUIRED)
find_package(sha256 REQUIRED)

sel4_import_kernel()
elfloader_import_project()

# This sets up environment build flags and imports musllibc and runtime libraries.
musllibc_setup_build_environment_with_sel4runtime()
sel4_import_libsel4()
util_libs_import_libraries()
sel4_libs_import_libraries()
accprof_soft_import_library()
sha256_import_library()

if(ProfMetaFile)
    set(ProfMetaFlag "SHELL:-mllvm --prof-meta-file=${ProfMetaFile}")
endif()
if(EventShiftsFile)
    set(EventShiftsFlag "SHELL:-mllvm --event-shifts-file=${EventShiftsFile}")
endif()
if(KeysFile)
    set(KeysFlag "SHELL:-mllvm --keys-file=${KeysFile}")
endif()
# add_executable(app src/app.c)

# # Include the pass in the app elf file
# #target_compile_options(app PRIVATE -fpass-plugin=/home/mskordal/workspace/myRepos/my-sel4-projects/llvmpassbuild/libInjectBRAM.so)

# target_link_libraries(app
# 	sel4 muslc sel4muslcsys
# 	sel4utils)

# include(cpio)
# MakeCPIO(archive.o "$<TARGET_FILE:app>")

# add_executable(stream archive.o src/main.c)
add_executable(stream src/main.c)

target_compile_options( stream PRIVATE 
						-O0
                        -fplugin=${LLVMPass}
                        -fpass-plugin=${LLVMPass}
                        "SHELL:-mllvm --functions-file=${FunctionsFile}"
                        "SHELL:-mllvm --events-file=${EventsFile}"
                        "${ProfMetaFlag}"
                        "${EventShiftsFlag}"
                        "${KeysFlag}"
)

target_link_libraries(stream 
    sel4
    muslc
    utils
    sel4muslcsys
    sel4platsupport
    sel4utils
    sel4debug
    sel4allocman
    sel4bench
    accprof_soft
    sha256
)


include(rootserver)
DeclareRootserver(stream)

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
