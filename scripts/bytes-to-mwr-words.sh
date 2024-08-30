#!/bin/bash

# Accepts one argument where every line is a sequence of 32 hex bytes separated
# by <space>. For each line outputs a series of mwr commands that write 

of=mwrs.tcl
hls_addr=0xa0000020

# Check if directory path is provided
if [ -z "$1" ]; then
	echo "Usage: ./bytes-to-mwr-words.sh /path/to/bytes/file"
	exit 1
fi

rm $of
while IFS= read -r line; do
	read -a byte_arr <<< "$line"
	byte_arr_size=${#byte_arr[@]}
	for ((word_idx=0; word_idx<byte_arr_size; word_idx+=4)); do
		word=""
		for ((byte_idx=0; byte_idx<4; byte_idx++)); do
			if ((word_idx + byte_idx < byte_arr_size)); then
				byte="${byte_arr[word_idx + byte_idx]}"
				if [[ ${#byte} -eq 1 ]]; then
					byte="0$byte"
				fi
				word="${byte}${word}"
			else
				word="00$word"
			fi
		done
		printf "mwr -force 0x%x 0x%s\n" "$hls_addr" "$word" >> $of
		hls_addr=$((hls_addr + 4))
	done
	printf "\n" >> $of
	hls_addr=0xa0000020
done < $1
