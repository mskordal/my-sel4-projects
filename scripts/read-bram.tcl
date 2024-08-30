set filename [lindex $argv 0]
set logfile [open $filename "w"]

connect
targets -set -filter {name =~ "PSU"}
# Set range accordingly if you don't need to read all of BRAM. Will make the
# read process faster.
for {set bramAddr 0xa0040000} {$bramAddr < 0xa0050000} {set bramAddr [expr {$bramAddr+0x4}]} {
	set data [mrd -force $bramAddr]
	puts $logfile $data
}
close $logfile
