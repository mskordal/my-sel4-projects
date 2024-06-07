# Dummy Applications
This is directory that contains simple c prorams that can be used for various
reasons mainly testing. E.g. if one wants to implement an algorithm in IR they
can write the algorithm in C and compile the code with clang to see how the C
code is deconstructed to IR. Another example would be C files that could be used
as simple tests for the root thread of an SeL4 project.

# Current Files
- **app.c**: A dummy standalone program used to test the pass.

- **gfc.c**: A dummy program that calls a sequence of functions that do
  nothing. It is used to test call graph generation and the pass functionality.

- **gfcmod.c**: The `gfc.c` file with added c instructions that operate on the
  HLS IP. It is used to translate it with clang to IR code, and then implement
  those IR instructions in the pass for injection.

- **pointer-function.c**: Probably used to understand how IR works with
  pointers.
