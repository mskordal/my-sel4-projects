connect
targets -set -filter {name =~ "PSU"}
mwr 0xffca0038 0x1ff
targets -set -filter {name =~ "MicroBlaze*"}
dow pmufw.elf
con
target -set -filter {name =~ "Cortex-A53 #0*"}
rst -processor
dow zynqmp_fsbl.elf
con
# Add some delay after con to give it time to load fsbl
after 1000
dow bl31.elf
con
after 1000
dow u-boot.elf
con
# Load the seL4 image in ELF format, the file can be found in the sel4test build directory
dow sel4build/elfloader/elfloader
con
targets -set -filter {name =~ "PSU"}
