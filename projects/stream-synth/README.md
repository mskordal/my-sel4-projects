# STREAM Synthetic Benchmark
This project is almost identical to [stream on
SeL4](https://github.com/mskordal/my-sel4-projects/tree/main/projects/stream).
The original code can be found at
[STREAM](https://github.com/jeffhammond/STREAM). The only difference is that
instead of the `stream_func` function calling all other 4 functions serially, we
change the call patterns to create different call trees. Used mainly to fish
different metric results when profiling.
