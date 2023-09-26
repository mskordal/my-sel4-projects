#!/bin/bash

arr=($(awk '
{
	if((NR-1)%32==0)
	{
		if($2 == 0)
			exit
	}
	if(NR%2==1)
	{
		print $2
	}
	
}' $1))

printf	"function-id,function-lvl,cpu-cycles,event-0,event-1,event-2," > cg.csv
printf	"event-3,event-4,event-5\n"  >> cg.csv
for ((i=0; i<${#arr[@]}; i+=16));
do
	printf "%d," $((16#${arr[i]} & 0xFF)) >> cg.csv
	printf "%d," $((16#${arr[i+1]} & 0xFF)) >> cg.csv
	printf "%d," $((16#"${arr[i+3]}${arr[i+2]}")) >> cg.csv
	printf "%d," $((16#"${arr[i+5]}${arr[i+4]}")) >> cg.csv
	printf "%d," $((16#"${arr[i+7]}${arr[i+6]}")) >> cg.csv
	printf "%d," $((16#"${arr[i+9]}${arr[i+8]}")) >> cg.csv
	printf "%d," $((16#"${arr[i+11]}${arr[i+10]}")) >> cg.csv
	printf "%d," $((16#"${arr[i+13]}${arr[i+12]}")) >> cg.csv
	printf "%d" $((16#"${arr[i+15]}${arr[i+14]}")) >> cg.csv
	printf "\n" >> cg.csv
done