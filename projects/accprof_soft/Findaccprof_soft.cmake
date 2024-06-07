set(ACCPROF_SOFT_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE STRING "")
mark_as_advanced(ACCPROF_SOFT_DIR)

macro(accprof_soft_import_library)
    add_subdirectory(${ACCPROF_SOFT_DIR} accprof_soft)
endmacro()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(accprof_soft DEFAULT_MSG ACCPROF_SOFT_DIR)