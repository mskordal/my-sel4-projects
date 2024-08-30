set filename [lindex $argv 0]
set logfile [open $filename "w"]

connect
targets -set -filter {name =~ "PSU"}
# KEYS to send to hardware at first trigger. Write these to BRAM
# 0x18 1afd5fb0
# 0x81 ebd55fb4
# 0x10 3b7a9fb4
# 0x20 7b725fb4
# 0x30 3bfb3fb4
mwr -force 0xa0040000 0x1afd5fb0
mwr -force 0xa0040004 0x18

mwr -force 0xa0040030 0xebd55fb4
mwr -force 0xa0040034 0x81

mwr -force 0xa0040060 0x3b7a9fb4
mwr -force 0xa0040064 0x10

mwr -force 0xa0040090 0x7b725fb4
mwr -force 0xa0040094 0x20

mwr -force 0xa00400C0 0x3bfb3fb4
mwr -force 0xa00400C4 0x30

# Set range accordingly if you don't need to read all of BRAM. Will make the
# read process faster.
mwr -force 0xa0000040 0xa0040000
#mwr -force 0xa0000000 0x1
set ctrl_val [mrd -force -value 0xa0000000]
set ctrl_val [expr $ctrl_val | 0x1]
mwr -force 0xa0000000 $ctrl_val
after 2000


mwr -force 0xa0000020 0xb00ff0a4
mwr -force 0xa0000024 0x05ec7b11
mwr -force 0xa0000028 0x313eb9b2
mwr -force 0xa000002c 0x144d47ce
mwr -force 0xa0000030 0x1f6b40db
mwr -force 0xa0000034 0x06c3aea5
mwr -force 0xa0000038 0xe75203b5
mwr -force 0xa000003c 0x22d71bf2
set ctrl_val [mrd -force -value 0xa0000000]
set ctrl_val [expr $ctrl_val | 0x1]
mwr -force 0xa0000000 $ctrl_val
after 2000

mwr -force 0xa0000020 0xc453845e
mwr -force 0xa0000024 0x6e425f5a
mwr -force 0xa0000028 0x844ebed3
mwr -force 0xa000002c 0x9bcd16e0
mwr -force 0xa0000030 0x207dda00
mwr -force 0xa0000034 0x154c0000
mwr -force 0xa0000038 0xa7c02af0
mwr -force 0xa000003c 0x8aa08d2b
set ctrl_val [mrd -force -value 0xa0000000]
set ctrl_val [expr $ctrl_val | 0x1]
mwr -force 0xa0000000 $ctrl_val
after 2000

mwr -force 0xa0000020 0xefb0eb30
mwr -force 0xa0000024 0x8c87d766
mwr -force 0xa0000028 0x6fda7624
mwr -force 0xa000002c 0xa01d7208
mwr -force 0xa0000030 0x2d20bcbb
mwr -force 0xa0000034 0xf151cda3
mwr -force 0xa0000038 0xbf17ceea
mwr -force 0xa000003c 0xea948042
set ctrl_val [mrd -force -value 0xa0000000]
set ctrl_val [expr $ctrl_val | 0x1]
mwr -force 0xa0000000 $ctrl_val
after 2000

mwr -force 0xa0000020 0xcc7651ac
mwr -force 0xa0000024 0x50439380
mwr -force 0xa0000028 0x093da911
mwr -force 0xa000002c 0x066865b0
mwr -force 0xa0000030 0xeaac86b4
mwr -force 0xa0000034 0x1f169986
mwr -force 0xa0000038 0x696c7417
mwr -force 0xa000003c 0x9639bb9d
set ctrl_val [mrd -force -value 0xa0000000]
set ctrl_val [expr $ctrl_val | 0x1]
mwr -force 0xa0000000 $ctrl_val
after 2000

mwr -force 0xa0000020 0xda0d0fd4
mwr -force 0xa0000024 0xf8afa5ba
mwr -force 0xa0000028 0x241b537e
mwr -force 0xa000002c 0x7918c997
mwr -force 0xa0000030 0x00bc00e9
mwr -force 0xa0000034 0xe94e4967
mwr -force 0xa0000038 0x9d9f14a6
mwr -force 0xa000003c 0x88c4f006
set ctrl_val [mrd -force -value 0xa0000000]
set ctrl_val [expr $ctrl_val | 0x1]
mwr -force 0xa0000000 $ctrl_val
after 2000

for {set bramAddr 0xa0040000} {$bramAddr < 0xa0040188} {set bramAddr [expr {$bramAddr+0x4}]} {
	set data [mrd -force $bramAddr]
	puts $logfile $data
}
close $logfile
