# my-sel4-projects
- Different SeL4 standalone projects for different purposes
- An LLVM pass project for instruction injection

# Pre-requirements
Install host dependencies from here:
<https://docs.sel4.systems/projects/buildsystem/host-dependencies.html>

Tutorials can be found here:
<https://docs.sel4.systems/Tutorials/>

Prebuilt projects (SeL4Test is a good project to test functionality):
<https://docs.sel4.systems/projects/>

Latest TRM can be found here:
<https://sel4.systems/Info/Docs/seL4-manual-latest.pdf>

API reference:
<https://docs.sel4.systems/projects/sel4/api-doc.html>

# Build the project structure
To create a starting project with a simple skeleton, and to learn how to build
and run a project, follow the instructions of the [Hello
World](https://github.com/mskordal/my-sel4-projects/tree/main/projects/hello)
project in this repository.

# Current projects
- [Hello
  World](https://github.com/mskordal/my-sel4-projects/tree/main/projects/hello):
  A simple project that prints 'Hello World' to standard output.
- [Mapping
  Project](https://github.com/mskordal/my-sel4-projects/tree/main/projects/mapping):
  A project that maps device memory to an application.
- [Mapping Project using
  VKA](https://github.com/mskordal/my-sel4-projects/tree/main/projects/vkamapping):
  Another project that maps device memory to an application, however it uses
  dynamic libraries for a more automated procedure.
- [Inject BRAM access
  instructions](https://github.com/mskordal/my-sel4-projects/tree/main/myLlvmPass):
  An LLVM pass that injects instructions to application for reading and writing
  to BRAM.

# TCL scripts
The root directory contains several tcl scripts for hardware boot and
communication. Those are explained below:

## init-board.tcl & init-boardhw.tcl
Those scripts are used to load the initial boot files for booting the fpga and
sel4 on to the ZCU. The only difference is that `init-boardhw.tcl` does not
include the two pre-last instructions for loading the `elfloader` image. This is
for when we want to test the hardware design without sel4. In that case we would
usually read or write manually to the hardware addresses through `xsct`.

## read-bram.tcl
This script is used to read all the data from the BRAM in the FPGA and write
them to a txt file. That way we can pull the call graph data in the BRAM for
evaluation.