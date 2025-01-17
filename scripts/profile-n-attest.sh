#!/bin/bash

set -e

# $1: How many times to run the application for profiling
# $2: For how many variances to run the application
# $3: How many times to run each variance
# $4: ON if compiling large project

root_path="/home/mskordal/workspace/myRepos/my-sel4-projects"
script_path="${root_path}/scripts"

profile_execs=$1
attest_execs=$2
var_reps=$3
compile_large=$4

${script_path}/run-profile.sh ${profile_execs} ${compile_large}
${script_path}/run-attest.sh ${attest_execs} ${var_reps} ${compile_large}
