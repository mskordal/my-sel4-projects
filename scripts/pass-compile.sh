#!/bin/bash

# $1: The relative path to the directory of the pass source files
# The next 3 args are optional and don't need to be passed in a specific order
# $2: -DUSE_CLANG=ON : Pass is built to be used with clang
# $3: -DUSE_HLS=ON : Pass is built to communicate with HLS (AccProf only)
# $4: -DDEBUG_COMP_PRINT=ON : Enable debug messages during compilation
current_dir=$(basename "$PWD")

# Check if the directory name is exactly "build" or ends with "build"
if [[ ! "$current_dir" =~ build$ ]]; then
  echo "Error: Directory must be called or end in 'build'."
  exit 1
fi

if [[ $# -eq 0 ]] ; then
	echo 'No arguments provided!'
	echo -e '\t$1: The relative path to the directory of the pass source files.'
	exit 1
fi

export LLVM_DIR=/usr/lib/llvm-15
rm -rf ./*
cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR "$@"
make
