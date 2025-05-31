set script_dir [file dirname [info script]]
set functions_file_path [file join $script_dir "../accProfPass/functions.txt"]
set functions_file [open $functions_file_path "r"]

set pmufw [lindex $argv 0]
set fsbl [lindex $argv 1]
set bl31 [lindex $argv 2]
set uboot [lindex $argv 3]
set arg_4 [lindex $argv 4]
set keys_file [open $arg_4 "r"]

# Read contents to lists
set functions [split [string trim [read $functions_file]] "\n"]
set keys [split [string trim [read $keys_file]] "\n"]

# Close the files
close $functions_file
close $keys_file

connect
targets -set -filter {name =~ "PSU"}
mwr 0xffca0038 0x1ff


targets -set -filter {name =~ "MicroBlaze*"}
dow ${pmufw}
con
target -set -filter {name =~ "Cortex-A53 #0*"}
rst -processor
dow ${fsbl}
con
# Add some delay after con to give it time to load fsbl
after 1000
dow ${bl31}
con
after 1000

targets -set -filter {name =~ "PSU"}
set bram_addr 0xa0040000
foreach function_line $functions key_line $keys {
	scan $function_line "%s %d" func_name func_id
	# write function id in first byte then progress 16 bytes so the 48 rest can
	# be used to write the key
	puts $func_id
	mwr -size b -force $bram_addr $func_id
	set bram_addr_tmp [expr {$bram_addr+0x8}]
	# put key into a list of bytes
	set key_line [string range $key_line 2 end]
	set key_byte_list [list]
	while {$key_line != ""} {
		set key_str_byte [string range $key_line end-1 end]
		scan $key_str_byte "%x" key_byte
		lappend key_byte_list $key_byte
		set key_line [string range $key_line 0 end-2]
	}
	puts $key_byte_list
	# write list to bram byte by byte
	foreach key_byte $key_byte_list {
		mwr -size b -force $bram_addr_tmp $key_byte
		set bram_addr_tmp [expr {$bram_addr_tmp+0x1}]
	}
	set bram_addr [expr {$bram_addr+0x40}]
}

target -set -filter {name =~ "Cortex-A53 #0*"}
after 1000
dow ${uboot}
con
after 1000

dow sel4build/elfloader/elfloader
con
targets -set -filter {name =~ "PSU"}
