set bitStream [lindex $argv 0]

open_hw_manager
connect_hw_server -allow_non_jtag

open_hw_target

set_property PROBES.FILE {} [get_hw_devices xczu9_0]
set_property FULL_PROBES.FILE {} [get_hw_devices xczu9_0]
set_property PROGRAM.FILE $bitStream [get_hw_devices xczu9_0]
current_hw_device [get_hw_devices xczu9_0]
program_hw_devices [get_hw_devices xczu9_0]
refresh_hw_device [lindex [get_hw_devices xczu9_0] 0]
