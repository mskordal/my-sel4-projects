#!/bin/bash

vivado -mode 'batch' -source scripts/program-dev.tcl
xsct scripts/init-boardhw.tcl
xsct scripts/trigger-hls-read-bram.tcl res1.txt
