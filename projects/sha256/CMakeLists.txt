cmake_minimum_required(VERSION 3.7.2)

project(sha256_proj C)

if(KernelArchRiscV)
    set(kernel_arch arm)
else()
    set(kernel_arch ${KernelArch})
endif()


# list(APPEND sources src/sha256.c)
file(
    GLOB
		yolo
        src/*.c
)

list(SORT yolo)

add_library(sha256 EXCLUDE_FROM_ALL ${yolo})

# target_link_libraries(sha256 PUBLIC -Wl,--retain-symbols-file="${CMAKE_CURRENT_SOURCE_DIR}/symbols.txt")

target_include_directories(
	sha256
	PUBLIC
		include
		src
)

target_link_libraries(sha256 muslc utils sel4 sel4_autoconf)