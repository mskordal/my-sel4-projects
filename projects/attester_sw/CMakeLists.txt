cmake_minimum_required(VERSION 3.7.2)

project(attester_sw_proj C)

if(KernelArchRiscV)
    set(kernel_arch arm)
else()
    set(kernel_arch ${KernelArch})
endif()


# list(APPEND sources src/attester_sw.c)
file(
    GLOB
		yolo
        src/*.c
)

list(SORT yolo)

add_library(attester_sw EXCLUDE_FROM_ALL ${yolo})

# target_link_libraries(attester_sw PUBLIC -Wl,--retain-symbols-file="${CMAKE_CURRENT_SOURCE_DIR}/symbols.txt")

target_include_directories(
	attester_sw
	PUBLIC
		include
		src
)

target_link_libraries(attester_sw muslc utils sel4 sel4_autoconf)