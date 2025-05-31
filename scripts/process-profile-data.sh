#!/bin/bash

root_path="/home/mskordal/workspace/myRepos/my-sel4-projects"
script_path="${root_path}/scripts"
events_file_path="${root_path}/accProfPass/events.txt"
data_path=$1
variances_i=$2
input_file="${data_path}/res0.csv"

${script_path}/cg-data-to-csv.sh ${data_path} ${events_file_path}
python ${script_path}/generate-keys.py ${input_file} ${variances_i}

