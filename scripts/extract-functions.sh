#!/bin/bash

output_file="functions.tx"
# Pase the source file and generate a tags file with the function names only
ctags -o $output_file --fields=+n --c-kinds=f $1 
# print only the first column except when starting with ! and add a number next
# to it starting at 1
awk '!/^!/ {print $1, ++count}' $output_file > $output_file"t"
rm $output_file


