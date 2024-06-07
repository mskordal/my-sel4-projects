cmake_minimum_required(VERSION 3.7.2)

project(accprof_soft_proj C)

if(KernelArchRiscV)
    set(kernel_arch arm)
else()
    set(kernel_arch ${KernelArch})
endif()


# list(APPEND sources src/accprof_soft.c)
file(
    GLOB
		yolo
        src/*.c
)

list(SORT yolo)

add_library(accprof_soft EXCLUDE_FROM_ALL ${yolo})

# target_link_libraries(accprof_soft PUBLIC -Wl,--retain-symbols-file="${CMAKE_CURRENT_SOURCE_DIR}/symbols.txt")

target_include_directories(
	accprof_soft
	PUBLIC
		include
		src
)

target_link_libraries(accprof_soft muslc utils sel4 sel4_autoconf)