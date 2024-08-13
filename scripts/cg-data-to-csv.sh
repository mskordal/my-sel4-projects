#!/bin/bash

# Check if directory path is provided
if [ -z "$1" ] || [ -z "$2" ]; then
	echo "Usage: ./cg-data-to-csv.sh /path/to/directory /path/to/events/file"
	exit 1
fi

DIR="$1"
mapfile -t event_names < "$2"
# Iterate over each .txt file in the directory
for txt_file in "$DIR"/*.txt; do
	# Check if there are no .txt files
	if [ ! -e "$txt_file" ]; then
		echo "No .txt files found in the directory."
		exit 1
	fi

    # Determine the corresponding .csv file name
    csv_file="${txt_file%.txt}.csv"
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
		
	}' "$txt_file"))

	printf "function-id,function-lvl,cpu-cycles,%s,%s,%s,%s,%s,%s\n" \
		"${event_names[0]}" "${event_names[1]}" "${event_names[2]}" \
		"${event_names[3]}" "${event_names[4]}" "${event_names[5]}" > "$csv_file"
	for ((i=0; i<${#arr[@]}; i+=16));
	do
		printf "%d," $((16#${arr[i]} & 0xFF)) >> "$csv_file"
		printf "%d," $((16#${arr[i+1]} & 0xFF)) >> "$csv_file"
		printf "%d," $((16#"${arr[i+3]}${arr[i+2]}")) >> "$csv_file"
		printf "%d," $((16#"${arr[i+5]}${arr[i+4]}")) >> "$csv_file"
		printf "%d," $((16#"${arr[i+7]}${arr[i+6]}")) >> "$csv_file"
		printf "%d," $((16#"${arr[i+9]}${arr[i+8]}")) >> "$csv_file"
		printf "%d," $((16#"${arr[i+11]}${arr[i+10]}")) >> "$csv_file"
		printf "%d," $((16#"${arr[i+13]}${arr[i+12]}")) >> "$csv_file"
		printf "%d" $((16#"${arr[i+15]}${arr[i+14]}")) >> "$csv_file"
		printf "\n" >> "$csv_file"
	done
	echo "Processed $txt_file -> $csv_file"
done
