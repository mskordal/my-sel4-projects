set logfile [open "cg-data.txt" "w"]

# Set range accordingly if you don't need to read all of BRAM. Will make the
# read process faster.
for {set bramAddr 0xa0040000} {$bramAddr < 0xa0043200} {set bramAddr [expr {$bramAddr+0x4}]} {
	set data [mrd -force $bramAddr]
	puts $logfile $data
}
close $logfile
