set(ATTESTER_SW_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE STRING "")
mark_as_advanced(ATTESTER_SW_DIR)

macro(attester_sw_import_library)
    add_subdirectory(${ATTESTER_SW_DIR} attester_sw)
endmacro()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(attester_sw DEFAULT_MSG ATTESTER_SW_DIR)