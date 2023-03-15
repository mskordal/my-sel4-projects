# LLVM InjectBRAM Pass
This is an LLVM Out-Of-Tree Pass for injecting code that reads and writes to
BRAM.

# Useful Information
Regarding syntax and the pass structure, visit the link:
<https://llvm.org/docs/WritingAnLLVMNewPMPass.html>

## Files
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

```

# Prev Pass State 1
The Pass just prints the non-external functions that are being called
in the programme.

# Current Pass State
Switched Function pass to a module pass. The pass create a global pointer variable and assigns the address `0xa0010000` to it
