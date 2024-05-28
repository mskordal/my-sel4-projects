# Embench benchmark results
The following directories contain evaluation gathered from running the Embench
IoT suite, using AccProf. They are represented in the form of `.csv` files.

# Separate
Results in this directory were gathered, by running each benchmark of the suite
separately on SeL4 on ZCU102. Benchmarks are relatively small in size and only
run on one function each. Since AccProf profiles on function granularity, only
one row is gathered per run for each corresponding benchmark. The rows were then
distributed in two files. `cpu1hw` contains the results when the benchmarks were
run with the hybrid version of AccProf and `cpu1sw` for the equivalent
software-only runs.

# All-in-one
In an attempt to create a large call-tree, we modified Embench to run all
benchmarks in one application sequentially. The results are diplayed in the
`.csv` files in this directory. The names of the files have the following
meaning: `embench-<1>-<2>-<3>_<4>.csv`
1. `hw` is when the benchmarks were run on AccProf. `sw` when run to the
   all-software version.
2. All benchmarks were run 92 times to create a call tree of ~2K nodes
3. There is a `CPU` define in each benchmark. Its value multiplies the amount of
   benchmark's computations. We tested for 1, 2 and 3.
4. Each configuration was run 3 times to get a heuristic estimate of the
   average.
The file ending in `all` was produced by a script that merged all 3 runs for the
same configuration in one file. We did that to import the data to a spreadsheet
document.