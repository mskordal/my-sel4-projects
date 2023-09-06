#!/bin/bash

rm -rf ./*
../init-build.sh -DPLATFORM=zcu102 -DTRIPLE=aarch64-linux-gnu
ninja
