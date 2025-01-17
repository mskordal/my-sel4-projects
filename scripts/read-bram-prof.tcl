set filename [lindex $argv 0]
set logfile [open $filename "w"]

connect
targets -set -filter {name =~ "PSU"}

for {set bramAddr 0xa0040000} {$bramAddr < 0xa007ffff} {set bramAddr [expr {$bramAddr+0x4}]} {
	set data [mrd -force $bramAddr]
	# Extract the data value (assumes format "A0040000:   06081202")
	set value [lindex [split $data ":"] 1]
	set value [string trim $value] ;# Trim any spaces

	# Check if the value is 0 and exit the loop
	if {[expr {$bramAddr % 0x40}] == 0} {
		if {$value eq "00000000"} {
			puts "Value at address [format "0x%08X" $bramAddr] is 0. Exiting loop."
			break
		}
	}
	# Write the original data string to the log file
	puts $logfile $data
}

close $logfile
