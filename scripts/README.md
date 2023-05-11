# Scripts for usage and automation

## extract-functions.sh
This script parses a C source file and outputs locally defined functions along
with an id for them separated by space. The main function is guar anteed to get
id 1 while every other function will get id >=2 determined by alphabetical
order. Output is written on the `functions.txt` file.

example output:
```txt
<func_name_x> 2
<func_name_y> 3
<func_name_z> 4
main 1
```

## pass.sh
This script is used to automate the building and testing procedure for the LLVM
pass. It must be run from the build directory used to build the pass. It can
perform the following actions:
1. Cleans the build directory, builds make files with cmake and creates an IR
   file called app.ll from app.c using clang.
2. Compiles the pass with make.
3. Injects the pass on the source file app.c using clang and produces app executable.
4. Injects the pass on the IR file app.ll using opt and produces app executable.

For instruction run with -h or open and view instructions in the beginning of
the script.