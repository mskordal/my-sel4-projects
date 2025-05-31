#!/bin/bash

# $1: For how many variances to run the application
# $2: How many times to run each variance
# $3: ON if compiling large projects
set -e

xilinx_settings_path="/home/mskordal/.local/Xilinx/Vivado/2024.1/settings64.sh"
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

var_execs=$1
var_reps=$2
of_prefix=res

function reset_board 
{
	usbrelay HURTM_1=1
	usbrelay HURTM_1=0
}
rm -rf ${outdir_path}
mkdir -p ${outdir_path}

source ${xilinx_settings_path}

for (( exec = 0 ; exec < var_execs ; exec++ ))
do
	# Generate keys
	if [ ! -d ${results_prof_path} ]; then
		echo "${results_prof_path} does not exist."
		exit 1
	fi
	if (( $(ls ${results_prof_path} | wc -l) > 0 )); then
		${script_path}/process-profile-data.sh ${results_prof_path} ${exec}
	else
		echo "Need a profile result file to generate keys"
		exit 1
	fi

	# Compile once for attestation
	cd ${sel4build_path}
	${script_path}/sel4-compile.sh ${att_pass_build_path}/libattprof.so \
		${prof_pass_path}/functions.txt ${prof_pass_path}/events.txt \
		${results_prof_path}/event-shifts.txt $3
	cd ${root_path}
	# repeat execution for the same variance
	for (( rep = 0 ; rep < var_reps ; rep++ ))
	do
		# Program FPGA
		vivado -mode 'batch' -source ${script_path}/program-dev.tcl \
			-tclargs ${bitstream_path}
		# Run attestation
		xsct ${script_path}/init-board-test.tcl ${elfs_path}/pmufw.elf \
			${elfs_path}/zynqmp_fsbl.elf ${elfs_path}/bl31.elf \
			${elfs_path}/u-boot.elf ${results_prof_path}/keys.txt 
		if [[ $? -ne 0 ]]; then # this tcl may error. If so, repeat iteration
			((rep--))
			reset_board
			continue
		fi
		# Wait execution to finish
		xsct ${script_path}/spinlock-bram.tcl
		# Read resuls from BRAM
		of=${outdir_path}/${of_prefix}${exec}_${rep}.txt
		xsct ${script_path}/read-bram-att.tcl ${of}
		reset_board
		sleep 1
	done
done
rm -rf ${root_path}/vivado*
