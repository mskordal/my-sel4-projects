#!/bin/bash

xilinx_settings_path=/home/mskordal/.local/Xilinx/Vivado/2024.1/settings64.sh
root_path="/home/mskordal/workspace/myRepos/my-sel4-projects"
script_path="${root_path}/scripts"
bitstream_path="${root_path}/bitstreams/attest.bit"
outdir_path="${root_path}/results-att"
elfs_path="${root_path}/att-elfs"

sel4build_path="${root_path}/sel4build"
prof_pass_build_path="${root_path}/profpassbuild"
att_pass_build_path="${root_path}/attpassbuild"

prof_pass_path="${root_path}/accProfPass"
results_prof_path="${root_path}/results-prof"

total_execs=$1
of_prefix=res
wait_exec_secs=$2

function reset_board 
{
	usbrelay HURTM_1=1
	usbrelay HURTM_1=0
}
rm -rf ${outdir_path}
mkdir -p ${outdir_path}

source ${xilinx_settings_path}

# Compile once for attestation
cd ${sel4build_path}
${script_path}/sel4-compile.sh ${att_pass_build_path}/libattprof.so \
	${prof_pass_path}/functions.txt ${prof_pass_path}/events.txt \
	${results_prof_path}/event-shifts.txt ${results_prof_path}/keys.txt
cd ${root_path}

for (( exec_num = 0 ; exec_num < total_execs ; exec_num++ ))
do
	vivado -mode 'batch' -source \
		${script_path}/program-dev.tcl -tclargs ${bitstream_path}
	xsct ${script_path}/init-board.tcl ${elfs_path}/pmufw.elf \
		${elfs_path}/zynqmp_fsbl.elf ${elfs_path}/bl31.elf \
		${elfs_path}/u-boot.elf
	of=${outdir_path}/${of_prefix}${exec_num}.txt
	sleep ${wait_exec_secs}
	xsct ${script_path}/read-bram.tcl ${of}
	reset_board
done
rm -rf ${root_path}/vivado*
