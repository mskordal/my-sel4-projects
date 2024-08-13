set filename [lindex $argv 0]
set logfile [open $filename "w"]

connect
targets -set -filter {name =~ "PSU"}
# Set range accordingly if you don't need to read all of BRAM. Will make the
# read process faster.
mwr -force 0xa0000040 0xa0040000
#mwr -force 0xa0000000 0x1
set ctrl_val [mrd -force -value 0xa0000000]
set ctrl_val [expr $ctrl_val | 0x1]
mwr -force 0xa0000000 $ctrl_val
after 1000
#mrd -force 0xa0000010
# write this hash to the correct offset before triggering the function, then
# read the bram byte and check whether is 1 or 0
# ce f5 20 4a 58 fc 8a 90
# b3 47 1  53 dc 5b c4 3c
# 82 7f b0 a4 7b a7 fe 9d
# d4 9d 95 67 42 81 60 f5
# 
# 3c 6c 7c 5b e6 2f 96 0d
# 35 bf 25 08 bd 8f ce 19
# c5 ad 25 1d b8 c2 e6 8c
# d8 b5 a7 37 e3 20 74 95
#
# d2 82 11 4b cb 2b 43 49
# 3e 96 3a 63 58 e3 a0 b6
# 64 27 9b 64 77 63 81 20
# d6 f7 ac 6c 90 fc 72 2e

mwr -force 0xa0000020 0x4a20f5ce
mwr -force 0xa0000024 0x908afc58
mwr -force 0xa0000028 0x530147b3
mwr -force 0xa000002C 0x3cc45bdc
mwr -force 0xa0000030 0xa4b07f82
mwr -force 0xa0000034 0x9dfea77b
mwr -force 0xa0000038 0x67959dd4
mwr -force 0xa000003C 0xf5608142
set ctrl_val [mrd -force -value 0xa0000000]
set ctrl_val [expr $ctrl_val | 0x1]
mwr -force 0xa0000000 $ctrl_val
after 1000

mwr -force 0xa0000020 0x5b7c6c3c
mwr -force 0xa0000024 0x0d962fe6
mwr -force 0xa0000028 0x0825bf35
mwr -force 0xa000002C 0x19ce8fbd
mwr -force 0xa0000030 0x1d25adc5
mwr -force 0xa0000034 0x8ce6c2b8
mwr -force 0xa0000038 0x37a7b5d8
mwr -force 0xa000003C 0x957420e3
set ctrl_val [mrd -force -value 0xa0000000]
set ctrl_val [expr $ctrl_val | 0x1]
mwr -force 0xa0000000 $ctrl_val
after 1000
#mwr -force -size b 0xa0000020 0xce 1
#mwr -force -size b 0xa0000021 0xf5 1
#mwr -force -size b 0xa0000022 0x20 1
#mwr -force -size b 0xa0000023 0x4a 1
#mwr -force -size b 0xa0000024 0x58 1
#mwr -force -size b 0xa0000025 0xfc 1
#mwr -force -size b 0xa0000026 0x8a 1
#mwr -force -size b 0xa0000027 0x90 1

#mwr -force -size b 0xa0000028 0xb3 1
#mwr -force -size b 0xa0000029 0x47 1
#mwr -force -size b 0xa000002a 0x01 1
#mwr -force -size b 0xa000002b 0x53 1
#mwr -force -size b 0xa000002c 0xdc 1
#mwr -force -size b 0xa000002d 0x5b 1
#mwr -force -size b 0xa000002e 0xc4 1
#mwr -force -size b 0xa000002f 0x3c 1

#mwr -force -size b 0xa0000030 0x82 1
#mwr -force -size b 0xa0000031 0x7f 1
#mwr -force -size b 0xa0000032 0xb0 1
#mwr -force -size b 0xa0000033 0xa4 1
#mwr -force -size b 0xa0000034 0x7b 1
#mwr -force -size b 0xa0000035 0xa7 1
#mwr -force -size b 0xa0000036 0xfe 1
#mwr -force -size b 0xa0000037 0x9d 1

#mwr -force -size b 0xa0000038 0xd4 1
#mwr -force -size b 0xa0000039 0x9d 1
#mwr -force -size b 0xa000003a 0x95 1
#mwr -force -size b 0xa000003b 0x67 1
#mwr -force -size b 0xa000003c 0x42 1
#mwr -force -size b 0xa000003d 0x81 1
#mwr -force -size b 0xa000003e 0x60 1
#mwr -force -size b 0xa000003f 0xf5 1

#set ctrl_val [mrd -force -value 0xa0000000]
#set ctrl_val [expr $ctrl_val | 0x1]
#mwr -force 0xa0000000 $ctrl_val
#after 1000

for {set bramAddr 0xa0040000} {$bramAddr < 0xa0040040} {set bramAddr [expr {$bramAddr+0x4}]} {
	set data [mrd -force $bramAddr]
	puts $logfile $data
}
close $logfile
