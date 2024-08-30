#!/bin/bash

xilinx_settings_path=/home/mskordal/.local/Xilinx/Vivado/2024.1/settings64.sh

source ${xilinx_settings_path}
vivado -mode 'batch' -source scripts/program-dev.tcl
xsct scripts/init-boardhw.tcl
xsct scripts/trigger-hls-read-bram.tcl reshw.txt
