# Embench benchmark results
This directory contains evaluation gathered from running the STREAM benchmark,
using AccProf. They are represented in the form of `.csv` files.

# .
The results running STREAM are diplayed in the `.csv` files in this directory.
The names of the files have the following meaning: `embench-<1>-<2>-<3>_<4>.csv`
1. `hw` is when the benchmarks were run on AccProf. `sw` when run to the
   all-software version.
2. we set the arrays to 1000, 10000 and 100000 elements size.
3. We ran the functions 500 times to get a call tree of 2K nodes
4. Each configuration was run 3 times to get a heuristic estimate of the
   average.
The file ending in `all` was produced by a script that merged all 3 runs for the
same configuration in one file. We did that to import the data to a spreadsheet
document.

# Stream-mod
Similar to STREAM, but we tried to make a modified version where `Copy` calls
`Scale` and `Add` and the two latter call `Triad` twice. This creates a tree of
7 nodes for each call to Copy, so we called Copy 285 times to create a get a
total call tree of ~2K nodes.