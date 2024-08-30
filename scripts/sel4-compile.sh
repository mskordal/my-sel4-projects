#!/bin/bash

# $1: The relative path to the LLVM pass used. 
# $2: The relative path to the text file containting the list of functions to be
# profiled.

# Check if the directory name is exactly "build" or ends with "build"
if [[ ! $(basename "$PWD") =~ build$ ]]; then
  echo "Error: Directory must be called or end in 'build'."
  echo "current dir: $PWD"
  exit 1
fi

if [[ $# -lt 2 ]] ; then
	echo 'No arguments provided!'
	echo -e '\t$1: The relative path to the LLVM pass used.'
	echo -e '\t$2: The relative path to the text file containting the list of'
	echo -e '\t    functions to be profiled.'
	exit 1
fi

export LLVM_DIR=/usr/lib/llvm-15
export LLVM_SYMBOLIZER_PATH=$LLVM_DIR/bin/llvm-symbolizer
rm -rf ./*
if [[ "$#" -eq 3 || "$#" -eq 4 ]]; then # profiling pass
	../init-build.sh -DPLATFORM=zcu102 -DTRIPLE=aarch64-linux-gnu \
		-DLLVMPass=$1 -DFunctionsFile=$2 -DEventsFile=$3 -DProfMetaFile=$4
elif [ "$#" -eq 5 ]; then # attestation pass
	../init-build.sh -DPLATFORM=zcu102 -DTRIPLE=aarch64-linux-gnu \
		-DLLVMPass=$1 -DFunctionsFile=$2 -DEventsFile=$3 -DEventShiftsFile=$4 \
		-DKeysFile=$5
fi
ninja
