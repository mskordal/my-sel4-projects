#!/bin/bash

# Check if directory path is provided
if [ -z "$1" ]; then
	echo "Usage: ./equal-key-percent.sh /path/to/directory"
	exit 1
fi

dir="$1"

for txt_file in "$dir"/*.txt; do

	# Run the awk command and store the results in an array
	array=($(awk 'NF > 0 {if ($2 == "00000000") exit; print $2}' $txt_file))

	# Concatenate all elements into a single string
	combined=$(printf "%s" "${array[@]}")
	count4=$(echo "$combined" | grep -o "4" | wc -l)
	count2=$(echo "$combined" | grep -o "2" | wc -l)
	percent=$(((100 * count4)/(count4 + count2)))
	echo "$(basename $txt_file) key equality: $percent%"
done
