set termin_word 0xa007FFFC

connect
targets -set -filter {name =~ "PSU"}

while {1} {
	after 4000
	set data [mrd -force $termin_word]
	# Extract the data value (assumes format "A0040000:   06081202")
	set value [lindex [split $data ":"] 1]
	set value [string trim $value] ;# Trim any spaces
	if {$value eq "00000001"} {
		break
	}
	puts "spinlocking bram"
}
