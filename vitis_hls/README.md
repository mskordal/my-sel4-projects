# Vitis HLS IP
This directory contains the source files required to build a hardware IP on
Vitis HLS. The IP is used on ZCU102 in conjuction with the SeL4 application and
the LLVM pass.

# Current version
The IP receives 2 values from the software application. An index and a hardware
address that corresponds to the BRAM, where the IP writes.
BRAM is split to 2 parts.
1. Part 1 is where the call graph is stored. When a positive ID arrives it means
   a functions has just been called. On every two bytes, the IP stores the
   function ID on the first byte, and the level of the function in the call
   graph on the second byte.
2. Part 2 is used as a stack to track how deep we are in the call stack and who
   is the parent function of each function that is called. When an ID > 0
   arrives, the index of where the function is stored in the call graph, is
   pushed in the stack. When ID==0 arrives, the stack is popped indicating that
   a function has just returned.

# Previous version 1
The IP receives 2 values from the software application. An index and a hardware
address that corresponds to the BRAM. When starting the IP through the control
register, the IP will increment a specific location in BRAM based on the index
received.

# Build and run
1. Open Vitis HLS.
2. Click on File -> New Project
3. Assign project name and location and press Next.
4. Give a Top Function name. This should be the same as the name of your IP main
   function. Press Next.
5. Do not add or create testbench files. Press Next.
6. Give a solution name and on Part Selection choose ZCU102. Finish
7. In the Explorer Pane on the right, create new source files under `Source`
8. Finally, press `Run Flow` in the top menu(green play button).

The exported RTL can be then added into the vivado hardware design.