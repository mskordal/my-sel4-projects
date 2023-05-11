#!/bin/bash

output_file="functions.tx"
# Pase the source file and generate a tags file with the function names only
ctags -o $output_file --fields=+n --c-kinds=f $1 
# print only the first column except when starting with ! and add a number next
# to it starting at 1
awk '!/^!/ {if($1 == "main"){print $1, 1} else {print $1, ++count + 1}}' $output_file > $output_file"t"
# vim +'norm GddggPZZ' $output_file"t" 
rm $output_file


