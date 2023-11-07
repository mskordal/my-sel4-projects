set logfile [open "cg-data.txt" "w"]

for {set bramAddr 0xa0020000} {$bramAddr < 0xa0040000} {set bramAddr [expr {$bramAddr+0x4}]} {
	set data [mrd -force $bramAddr]
	puts $logfile $data
}
close $logfile
