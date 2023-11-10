# LLVM InjectBRAM Pass
This is an LLVM Out-Of-Tree Pass for injecting code that reads and writes to
BRAM.

# Useful Information
Regarding syntax and the pass structure, visit the link:
<https://llvm.org/docs/WritingAnLLVMNewPMPass.html>

# Files
All files required to build the LLVM Pass, reside in
[myLllvmPass](https://github.com/mskordal/my-sel4-projects/tree/main/myLlvmPass).
- **CMakeLists.txt**: Passes are built using `cmake` and then `make`.

- **InjectBRAM.h**: The header file of our pass. Here we define the name of the
  pass in the form of a class and declare the `run` function which is the
  function that runs once the pass is enabled. All functions of our pass must
  be declared in the header file. They can later be called through the `run`
  function.

- **InjectBRAM.cpp**: The cpp file of our pass. All functionality of the pass
  is here for the time being. Apart from the functions that our pass uses to
  analyse or instrument code, we define `getInjectBRAMPluginInfo` of type
  `llvm::PassPluginLibraryInfo` and the code inside is used to register our
  Pass to the LLVM in order to run it withtools like `opt`.

- **app.c**: A dummy standalone programme used to test our pass.

- **gfc.c**: A dummy programme that calls a sequence of functions that do
  nothing. It is used to test call graph generation and the pass functionality.

- **gfcmod.c**: The `gfc.c` file with added c instructions that operate on the
  HLS IP. It is used to translate it with clang to IR code, and then implement
  those IR instructions in the pass for injection.

- **prof-func-list.md**: Contains presets of functions and IDs to quickly copy
  them to `functions.txt` depending on which application is being profiled.

# Build and run

```bash
# Create a variable that points to where your llvm is installed.
export LLVM_DIR=/usr/lib/llvm-15

# Create build directory and enter.
mkdir llvmpassbuild && cd llvmpassbuild

# Create the Makefiles and links to build the pass.
cmake -DLT_LLVM_INSTALL_DIR=$LLVM_DIR ../myLlvmPass

# Actually build the pass. It will be libInjectBRAM.so in our case.
make

# Compile the dummy programm into IR format.
$LLVM_DIR/bin/clang -O1 -S -emit-llvm ../myLlvmPass/app.c -o app.ll

# Use the plugin at the IR file. First case is for instrumentations passes
$LLVM_DIR/bin/opt -load-pass-plugin ./libInjectBRAM.so -passes=inject-bram app.ll -S -o derived.ll

# Second case is for analyses Passes
$LLVM_DIR/bin/opt -load-pass-plugin ./libInjectBRAM.so -passes=inject-bram -disable-output app.ll

# To directly inject the pass through clang without using opt, run the command
# below. Also for some reason this works even with -O0
$LLVM_DIR/bin/clang -O0 -fpass-plugin=./libInjectBRAM.so -g ../myLlvmPass/app.c -o app

```
# Pass functionality. Current version: 1.7
The pass profiles a set of functions on user-specified events, using the
hardware performance counters of the PMU results are stored as a conceptual
format of a call-graph. The results of 6 events and cpu cycles are stored in
each node of the call graph which represents a call instance of a profiling
function. The pass operates on 2 modes. On the first mode, the pass injects
instruction for writing to the HLS IP while on the second mode, it injects calls
to the software implementation of the HLS IP. By uncommenting/commenting
`#define USE_HLS` on can switch between those 2 modes. The define `DEBUG_PRINT`
enables the injection of printfs with desired data in the program at runtime for
debugging using the `insertPrint` function. The define `DEBUG_COMP_PRINT`
enables the output of debug messages at pass injection during the compilation of
an application using the `debug_errs` macro.

a53 cores which run on ZCU102 support up to 6 simultaneous events to count plus
CPU cycles. To choose the appropriate events, look over the list of events set
as defines, and then switch the defines `EVENT_0` to `EVENT_5` according to the
events you want to measure. CPU cycle counting is always active. The functions
that will be profiled are read from the `functions.txt` file. Each line in the
file must contain a function name followed by a unique integer id 1-255
separated by space. Check `prof-func-list.md` for examples.

## The pass performs the following actions to the application:
1. At global scope, it defines a global pointer set to the virtual address
   mapped to the HLS hardware and 7 global variables used to pass event counts
   from profiled callees to profiled callers. Note that for now, The application
   needs to map the HLS hardware on that same virtual address as the one that
   the pass uses. the Pass also creates function declarations of functions that
   are not initially visible by the application module, but the pass requires
   them to inject call instructions to those functions.
2. In the beginning of the main function it initializes the performance counters
   and sets them to specified `EVENT_0` to `EVENT_5` events.
The next steps apply for every function listed in `functions.txt`:
1. In the function prologue it defines local variables, and writes the function
   ID and event IDs to the HLS. If `USE_HLS` is set, the pass spins on the idle
   bit of the hardware to make sure multiple writes will not overlap and corrupt
   the data. It then resets the counters to start counting from the beginnig of
   the function.
3. In the body of each function, the pass searches for calls to those same
   functions. Before each call instruction, the pass stores the current event
   values since when the callee will be called, the counters will be reset. Then
   right after the call, the pass adds the callee's results to the currently
   stored values of the caller and resets the counters to resume execution.
4. In the function epilogue, the pass writes current event counts to the HLS
   hardware and saves the values to be passed to the function's caller as well.
5. For the software HLS mode, the pass prints the call graph in a csv format at
   epilogue of main.

# Version 1.6 changes
Pass works on multiple files that can be profiled. For each file, unique
pointers and globals are created. Fixed an issue where the pass would search for
main function in every file causing stack dump.

# Version 1.6 changes
1. The function `makeExternalDeclarations` is implemented. It automatically
   checks which functions that are going to be called by the pass are visible by
   the module. For those that are not, it creates external declerations to them
   so they can be injected at compile-time before linkining with objects that
   containt the function bodies.
2. Instead of writing event results of a function after it has returned, the
   results are written to hardware before return. This solves the issue where we
   needed to keep track of all the callers of the profiling functions, even if
   the callers were not supposed to be profiled.

# Version 1.5
The pass injects instructions to a source file at the prologue of each function
and after a function has returned. These instructions write to the HLS IP either
the function ID being called during the prologue or 0 to notify that a function
just returned. In particular, the pass performs the following steps:
1. It reads a text file that contains all of the locally defined functions of
   the source file with a positive integer ID unique to each function. Those are
   stored in two vectors
2. A global variable is created with a virtual address mapped on the physical
   address of the HLS design (must be mapped from within the app).
3. Then for every function registered from the file, the pass calls a sequence
   of functions that perform the following:
   1. `createCtrlSignalsLocalVarBlock`: Creates a basic block or uses one
      specified, and adds a local variable definition that is used to store the
      control signal values of the HLS IP.
   2. `createSpinLockOnIdleBitBlock`: Creates a basic block that spinlocks on
      the idle bit of the control signals until the idle bit is 1, meaning the
      HLS IP is available to write to.
   3. `createWriteToHLSBlock`: Creates a basic block that writes the physical
      address of the BRAM, the function ID or 0 and the required bits in the
      control signals in the corresponding addresses of the HLS IP. After this
      operation, the HLS function will start running.
   4. `addTerminatorsToBlocks`: Revisits the previous blocks and adds
      terminator instructions that mark the end of each block and the correct
      branch locations.
   5. `getCallInsts`: Retrieves all the call instructions to locally defined
      functions within the function the pass currently operates on.
4. Then for every every call instruction collected in each function, the pass
   first isolates those instructions to a block of their own and then calls the
   previous sequence of functions up to `addTerminatorsToBlocks`, in order to
   send to the HLS IP notice, that a function has just returned.

# Version 1.4
A global variable is created with a virtual address mapped on the physical
address of the HLS design (must be mapped from within the app). The pass adds a
code sequence on the prologue of each function and after every function call.
The sequence consists of 3 parts
1. The first block allocates a local variable
2. The second block spinlocks on the idle bit of the HLS control signals until
   the idle bit is 1
3. The third block writes a positive function ID at the function prologue or
   0 after a function call, the physical address of the BRAM and then sets
   to 1 the control bit of the control signals of the HLS which is supposed
   to send the data across. It is not tested yet

# Version 1.3
In main function, the address of the global is loaded to a local pointer, and
the then the constant value '1' is written to the address pointed by the local
pointer. This will cause a segmentation when run locally, since there is
nothing at that address.

# Version 1.2
Switched Function pass to a module pass. The pass creates a global pointer
variable and assigns the address `0xa0010000` to it.

# Version 1.1
The Pass just prints the non-external functions that are being called
in the programme.

# Trivia
If the pass operates on functions, there is a chance that the compiler will
optimize them out before the pass can inject any instruction. In that case
remember to compile with `-O0`.

# Notes

