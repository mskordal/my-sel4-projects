#!/bin/bash

# $1: How many times to run the application
# $2: How many seconds to wait for application execution before reading BRAM

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
prof_meta_file=prof-meta.txt
total_execs=$1
wait_exec_secs=$2

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
${prof_pass_path}/functions.txt ${prof_pass_path}/events.txt
cd ${root_path}

for (( exec_num = 0 ; exec_num < total_execs ; exec_num++ ))
do
	# load bitsream, bootloaders and image
	vivado -mode 'batch' -source \
		${script_path}/program-dev.tcl -tclargs ${bitstream_path}
	xsct ${script_path}/init-board.tcl ${elfs_path}/pmufw.elf \
		${elfs_path}/zynqmp_fsbl.elf ${elfs_path}/bl31.elf \
		${elfs_path}/u-boot.elf

	# wait a few seconds before reading bram to make sure app finished
	sleep ${wait_exec_secs}
	of=${outdir_path}/${of_prefix}${exec_num}.txt
	xsct ${script_path}/read-bram.tcl ${of}
	reset_board

	# very first execution is used to derive metadata
	if [[ ${exec_num} == 0 ]]; then
		${script_path}/cg-data-to-csv.sh ${outdir_path} \
			${prof_pass_path}/events.txt
		csv_of=${outdir_path}/${of_prefix}${exec_num}.csv
		lines_num=$(wc -l < ${csv_of})
		func_calls=$((lines_num - 1))
		echo ${func_calls} > ${outdir_path}/${prof_meta_file}
		rm -rf ${of} ${csv_of}
		cd ${sel4build_path}
		${script_path}/sel4-compile.sh ${prof_pass_build_path}/libAccProf.so \
		${prof_pass_path}/functions.txt ${prof_pass_path}/events.txt \
		${outdir_path}/${prof_meta_file}
		cd ${root_path}
	fi
done
rm -rf ${root_path}/vivado*

if (( total_execs < 3 )); then
	exit 0
fi

${script_path}/process-profile-data.sh ${outdir_path}
