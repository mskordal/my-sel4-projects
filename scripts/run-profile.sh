#!/bin/bash

# $1: How many times to run the application
# $2: ON if compiling large projects
set -e

xilinx_settings_path="/home/mskordal/.local/Xilinx/Vivado/2024.1/settings64.sh"
root_path="/home/mskordal/workspace/myRepos/my-sel4-projects"

sel4build_path="${root_path}/sel4build"
script_path="${root_path}/scripts"
bitstream_path="${root_path}/bitstreams/profile.bit"
outdir_path="${root_path}/results-prof"
elfs_path="${root_path}/prof-elfs"
prof_pass_path="${root_path}/accProfPass"
prof_pass_build_path="${root_path}/profpassbuild"

of_prefix=res
total_execs=$1

function reset_board 
{
	usbrelay HURTM_1=1
	usbrelay HURTM_1=0
}
rm -rf ${outdir_path}
mkdir -p ${outdir_path}

source ${xilinx_settings_path}

# compile the app for profiling
cd ${sel4build_path}
${script_path}/sel4-compile.sh ${prof_pass_build_path}/libAccProf.so \
${prof_pass_path}/functions.txt ${prof_pass_path}/events.txt "" "" $2
cd ${root_path}

for (( exec = 0 ; exec < total_execs ; exec++ ))
do
	# load bitsream, bootloaders and image
	vivado -mode 'batch' -source \
		${script_path}/program-dev.tcl -tclargs ${bitstream_path}
	xsct ${script_path}/init-board.tcl ${elfs_path}/pmufw.elf \
		${elfs_path}/zynqmp_fsbl.elf ${elfs_path}/bl31.elf \
		${elfs_path}/u-boot.elf
	# Wait execution to finish
	xsct ${script_path}/spinlock-bram.tcl
	# Read resuls from BRAM
	of=${outdir_path}/${of_prefix}${exec}.txt
	xsct ${script_path}/read-bram-prof.tcl ${of}
	reset_board
	sleep 1
done

# Second argument is variance. We don't care for that here as we simply want to
# output the metrics per of every function in its file
${script_path}/process-profile-data.sh ${outdir_path} 0

rm -rf ${root_path}/vivado*
