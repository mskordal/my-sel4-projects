#!/bin/bash

export LLVM_DIR=/usr/lib/llvm-15
export LLVM_SYMBOLIZER_PATH=$LLVM_DIR/bin/llvm-symbolizer
rm -rf ./*
../init-build.sh -DPLATFORM=zcu102 -DTRIPLE=aarch64-linux-gnu
ninja
