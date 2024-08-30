#!/bin/bash

# $1: How many times to run the application for profiling
# $2: How many times to run the application for attestation
# $3: How many seconds to wait for application execution before reading BRAM

root_path="/home/mskordal/workspace/myRepos/my-sel4-projects"
script_path="${root_path}/scripts"

profile_execs=$1
attest_execs=$2
wait_exec_secs=$3

${script_path}/run-profile.sh ${profile_execs} ${wait_exec_secs}
${script_path}/run-attest.sh ${attest_execs} ${wait_exec_secs}
