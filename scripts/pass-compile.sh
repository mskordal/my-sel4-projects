#!/bin/bash

# $1: The relative path to the directory of the pass source files
# The next 3 args are optional and don't need to be passed in a specific order
# $2: -DUSE_CLANG=ON : Pass is built to be used with clang
# $3: -DUSE_HLS=ON : Pass is built to communicate with HLS
# $4: -DDEBUG_COMP_PRINT=ON : Enable debug messages during compilation

if [[ $# -eq 0 ]] ; then
	echo 'No arguments provided!'
	echo -e '\t$1: The relative path to the directory of the pass source files.'
	exit 0
fi

export LLVM_DIR=/usr/lib/llvm-15
rm -rf ./*
cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR "$@"
make
