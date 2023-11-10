# Embench Benchmark Suite
This is a simple port of the STREAM benchmark for SeL4. The original code can be
found at [embench-iot](https://github.com/embench/embench-iot).

# Prerequisites
This project uses the mapping sequence of the
[vkamapping](https://github.com/mskordal/my-sel4-projects/tree/main/projects/vkamapping)
project to map the HLS hardware to the application so use that for reference on
how mapping pages works. After mapping the hardware, the main thread runs the
functions of all benchmarks.