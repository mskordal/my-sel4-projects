#!/bin/bash

# Arguments: variable number of csv files that containt similar metrics e.g.
# <csv_file>_1.csv <csv_file>_1.csv ... <csv_file>_n.csv. Finally it produces an
# output file named <csv_file>_all.csv. Uses the first input file as reference
# for the output name.

# Output file name
output_file=${1::-5}all.csv

# Merge columns using awk
awk -F , -v OFS=',' '{
		if(FNR==NR && NR>1){
			start[FNR]=$1 "," $2 ",";
			ev0[FNR] = $3
			ev1[FNR] = $4
			ev2[FNR] = $5
			ev3[FNR] = $6
			ev4[FNR] = $7
			ev5[FNR] = $8
			ev6[FNR] = $9
		}
		else if(FNR>1){
			ev0[FNR] = ev0[FNR] "," $3
			ev1[FNR] = ev1[FNR] "," $4
			ev2[FNR] = ev2[FNR] "," $5
			ev3[FNR] = ev3[FNR] "," $6
			ev4[FNR] = ev4[FNR] "," $7
			ev5[FNR] = ev5[FNR] "," $8
			ev6[FNR] = ev6[FNR] "," $9
		}
}
END {
	for(i in start)
	{
		print start[i] ev0[i] "," ev1[i] "," ev2[i] "," ev3[i] "," ev4[i] "," ev5[i] "," ev6[i]
	}
}' $@ > "$output_file"