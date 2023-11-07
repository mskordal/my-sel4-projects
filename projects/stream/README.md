# STREAM Benchmark
This is a simple port of the STREAM benchmark for SeL4. The original code can be
found at [STREAM](https://github.com/jeffhammond/STREAM).

# Prerequisites
This project uses the mapping sequence of the
[vkamapping](https://github.com/mskordal/my-sel4-projects/tree/main/projects/vkamapping)
project to map the HLS hardware to the application so use that for reference on
how mapping pages works. After mapping the hardware, the main thread runs a
function that contains the STREAM algorithm.

# Issues
STREAM uses gettimeofday to calculate bandwidth. `gettimeofday` calls
`clock_gettime` which has not yet implemented for Aarch64. We have commented out
the code that uses those system calls so the actual bandwidth is never
calculated. We still run the algorithm for profiling using our profiling
framework