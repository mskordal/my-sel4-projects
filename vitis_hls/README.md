# Vitis HLS IP
This directory contains the source files required to build a hardware IP on
Vitis HLS. The IP is used on ZCU102 in conjuction with the SeL4 application and
the LLVM pass.

# version 4 (current)
Works similarly to version 3 but with a few minor differences:
- Function paremeters for cpu cycles and events are divided to 2 32bits now.
- stack functions are now simplified and implemented inline with the main code.
- function level location in the NodeDataMS slot, has been moved from the MS
  byte to the LS byte to allow the hardware and the pass to avoid some bit shift
  operations.

# version 3
The IP now stores information about a function that is called additionally to
assembling a call graph. It receives a total of 8 64bit parameters split into
16 32bits. Here they will be described as 8 64bits. Those are:
- NodeData (Parameter 0): Each byte holds different information:
   - byte 0: Function ID
   - byte 1: Event 0 ID
   - byte 2: Event 1 ID
   - byte 3: Event 2 ID
   - byte 4: Event 3 ID
   - byte 5: Event 4 ID
   - byte 6: Event 5 ID
   - byte 7: Function level in the call stack
- cpyCycles (parameter 1): The number of CPU cycles gathered while this 
  function was running.
- event 0 - 5 (Parameters 2 - 7): The number of triggers that happened for each
  corresponding event while this function was running.
  
The IP is triggered in two different occasions and in each occasion it uses
different parameters. The 2 occasions are explained below:
1. When a function is called The pass writes in NodeData the Function and event
   ID for each byte. Function level is ignored as this is determined in the
   hardware. All other parameters are ignored in this case. The hardware stores
   NodeData some where in BRAM and leaves the next 14 bytes empty to be filled
   later. Then it updates the BRAM index to store nodeData for the next function
   when it arrives.
2. When a function terminates, the pass writes in NodeData byte 0 the number 0,
   indicating that the hardware was triggered due to function termination.
   Parameters for cpu cycles and events 0 - 5 are filled as well, then the
   hardware is triggered. The hardware then locates the index to where the
   nodeData of this function is located and fills the rest of the bytes with the
   cpu cycles and event information.

# version 2
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

# version 1
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