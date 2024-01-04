#!/bin/bash

# Arguments: 
#	$1 /path/to/functions.txt
#	$2 /path/of/csv/files This scripts takes
# Swithes first column from id to function name

# echo $2/*
for file in $2/*
do
# echo $file
	if [[ $file == *.old || $file == *count.csv ]]; then
		continue
	# else
	# 	echo $file
	fi
	cp $file $file.old
	awk -F '[ ,]' -v OFS=',' '{
		if(FNR==NR){
			name[$2]=$1
		}
		else if(FNR==1 && NR>1)
		{
			print
		}
		else if(FNR > 1)
		{
			$1=name[$1]; print
		}
	}' $1 $file  > $file.tmp
	# break
	mv $file.tmp $file
done