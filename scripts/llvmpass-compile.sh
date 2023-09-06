#!/bin/bash

export LLVM_DIR=/usr/lib/llvm-15
rm -rf ./*
cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ../myLlvmPass
make
