set pmufw [lindex $argv 0]
set fsbl [lindex $argv 1]
set bl31 [lindex $argv 2]
set uboot [lindex $argv 3]

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
dow ${uboot}
con
# Load the seL4 image in ELF format, the file can be found in the sel4test build directory
dow sel4build/elfloader/elfloader
con
targets -set -filter {name =~ "PSU"}
