#!/bin/bash

# Run this script from the build directory of the llvmPass.

__usage="
Usage: $(basename $0)
	[-i]: Clear build dir, make build files and create the IR file
	[-c]: Compile the pass
	[-r]: Inject the pass on the source file app.c using clang
	[-o]: Inject the pass on the IR file app.ll using opt
	[-z]: Do -c and -r 
	[-a]: Do -i, -c and -r
"

function init
{
	# clean build
	rm -rf ./*

	# point to llvm installation
	export LLVM_DIR=/usr/lib/llvm-15

	# build make files
	cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ../myLlvmPass

	# create IR file. Here we have removed optimizations. This may be an issue when
	# injecting code in functions. Remove the optnone flag in attributes definition
	# to be able to use the pass with -O0
	$LLVM_DIR/bin/clang -O0 -S -emit-llvm ../myLlvmPass/app.c -o app.ll
}

function compile
{
	# compile
	make
}

function runOpt
{
	# Use the plugin at the IR file.
	# First case is for instrumentations passes
	# Second case is for analyses Passes
	
	#$LLVM_DIR/bin/opt -load-pass-plugin ./libInjectBRAM.so -passes=inject-bram -disable-output app.ll
	$LLVM_DIR/bin/opt -load-pass-plugin ./libInjectBRAM.so -passes=inject-bram app.ll -S -o derived.ll
}

function runClang
{
	# Inject the pass directly with clang on a C source file and compile the app
	$LLVM_DIR/bin/clang -O0 -fpass-plugin=./libInjectBRAM.so -g ../myLlvmPass/app.c -o app
}

while getopts 'icrozah' opt; do
	case "$opt" in
		i)
			init
			;;
		c)
			compile
			;;
		r)
			runClang
			;;
		o)
			runOpt
			;;
		z)
			compile
			runClang
			;;			
		a)
			init
			compile
			runClang
			;;
		?|h)
			echo "$__usage"
			exit 1
			;;
	esac
done
shift "$(($OPTIND -1))"
