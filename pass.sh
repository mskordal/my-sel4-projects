#!/bin/bash

# Run this script from the build directory of the llvmPass.

__usage="
Usage: $(basename $0)
	[-i]: Clear build dir, make build files and create the IR file
	[-r]: Compile and run the pass
	[-a]: Do both above steps
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

function compileAndRun
{
	# compile
	make

	# Use the plugin at the IR file. First case is for instrumentations passes
	# Second case is for analyses Passes
	$LLVM_DIR/bin/opt -load-pass-plugin ./libInjectBRAM.so -passes=inject-bram app.ll -S -o derived.ll
	#$LLVM_DIR/bin/opt -load-pass-plugin ./libInjectBRAM.so -passes=inject-bram -disable-output app.ll
}

while getopts 'irah' opt; do
	case "$opt" in
		i)
			init
			;;
		r)
			compileAndRun
			;;
		a)
			init
			compileAndRun
			;;
		?|h)
			echo "$__usage"
			exit 1
			;;
	esac
done
shift "$(($OPTIND -1))"
