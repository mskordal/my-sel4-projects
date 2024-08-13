#!/bin/bash

# $1: The relative path to the LLVM pass used. 

# $2: The relative path to the text file containting the list of functions to be
# profiled.

if [[ $# -lt 2 ]] ; then
	echo 'No arguments provided!'
	echo -e '\t$1: The relative path to the LLVM pass used.'
	echo -e '\t$2: The relative path to the text file containting the list of'
	echo -e '\t    functions to be profiled.'
	exit 0
fi

export LLVM_DIR=/usr/lib/llvm-15
export LLVM_SYMBOLIZER_PATH=$LLVM_DIR/bin/llvm-symbolizer
rm -rf ./*
../init-build.sh -DPLATFORM=zcu102 -DTRIPLE=aarch64-linux-gnu -DLLVMPass=$1 -DFunctionsFile=$2 -DEventsFile=$3 -DEventShiftsFile=$4
ninja
