#!/bin/bash

total_execs=$1
script_path=/home/mskordal/workspace/myRepos/my-sel4-projects/scripts
of_prefix=res
wait_exec_secs=1
xilinx_settings_path=/home/mskordal/.local/Xilinx/Vivado/2024.1/settings64.sh

function reset_board 
{
	usbrelay HURTM_1=1
	usbrelay HURTM_1=0
}

source ${xilinx_settings_path}
for (( exec_num = 0 ; exec_num < total_execs ; exec_num++ ))
do
	vivado -mode 'batch' -source ${script_path}/program-dev.tcl
	xsct ${script_path}/init-board.tcl
	of=${of_prefix}${exec_num}.txt
	sleep ${wait_exec_secs}
	xsct ${script_path}/read-bram.tcl ${of}
	reset_board
done

