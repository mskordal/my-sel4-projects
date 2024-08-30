set(SHA256_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE STRING "")
mark_as_advanced(SHA256_DIR)

macro(sha256_import_library)
    add_subdirectory(${SHA256_DIR} sha256)
endmacro()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(sha256 DEFAULT_MSG SHA256_DIR)