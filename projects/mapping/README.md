# Mapping project (Map BRAM to SeL4)
This is a tutorial on how to map a physical address of the PL of a Xilinx
Ultrascale+ MPSoC to an SeL4 user application. More specifically we use the
ZCU102 Xilinx board as our setup environment.

# Prerequisites
Make sure you have read the [Hello World](https://github.com/mskordal/my-sel4-projects/tree/main/projects/hello) project of the same repository on instructions how to create the project structure, build and run the project.

For this tutorial we have used several tools and guides and have also installed
the dependecies of those tools:
- To setup a simple SeL4 standalone project, and use it as the foundation of
  our application, we followed the steps of the following repository:
  <https://github.com/mskordal/SeL4-hello-world>

- To load the SeL4 image on the ZCU102, we followed the following guide:
  <https://docs.sel4.systems/Hardware/ZCU102.html>

- To create a simple hardware design and some of the elf files described in the
  tutorial link above, we used Xilinx Vitis 2022.2 and Vivado 2022. Those can
  be downloaded from the following links:
  <https://www.xilinx.com/support/download.html>

# Guides
We consulted SeL4 documentation which contains useful tutorials, that guide you
on how to manage physical memory and map it to an application's virtual address
space.

- The following tutorial guides you on how to manage unmapped memory called
  Untyped in the SeL4 Universe
  <https://docs.sel4.systems/Tutorials/untyped.html>

- The following tutorial guides you on how to map untypped memory into an
  application's virtual address space with appropriate access rights.
  <https://docs.sel4.systems/Tutorials/mapping.html>

# Assumptions
We assume a BRAM in our hardware design starting at physical address 0xA0000000
with 8KB of size.

The SeL4 microkernel passes the root thread an array of capabilities to
available unmapped memory. Unmapped Memory is initially categorized into:
- **Untyped** which is DRAM unmapped memory

- **Device Untyped** which are regions corresponding to potentially added devices in
  our system which are currently unmapped.

# Strategy
For the ZCU102 board, the Kernel supplies the root thread a capability to a
device memory range from *0x80000000* to *0xC0000000*. Our BRAM lies in that
range from *0xA0010000* to *0xA0011FFF*. There are certain rules regarding the
mapping process in SeL4.

1. You can't directly map a subpart of an untyped region that does not start in
   the region's base address unless all of that region's preceding memory has
   been retyped in any way.

2. Every region mapped must be of size equal to a power of 2 number, by
   specifying the exponent that will produce the power of 2 result.

3. Every subpart mapped in the same region must be equal or smaller in size
   than a previously allocated subpart in the same region. Otherwise, an unused
   padding is allocated before that part, so that the the memory allocated in
   that region before that part, is it's multiple.

To allocate a frame starting from 0xA0010000 in the larger region above, we
first need to retype the *(0x80000000,0xA0000000)* subpart of our larger
region, then retype *(0xA0000000,0xA0010000)*, then finally map the frame at
*(0xA0010000,0xA0012000)* The first two parts will still be retyped as Untyped
since they do not correspond to any actual device and cannot be used in any
way. The third part will be typed as an actual memory frame.

# Files
To map a frame, we use a similar strategy to the mapping tutorial of SeL4. That
is why we copy the *main.c* file from that tutorial and build on top of that.
To access the completed main file of the mapping tutorial:
- follow the instructions from the following link to clone the tutorials
  directory:
  <https://docs.sel4.systems/Tutorials/#get-the-code>

- From the root directory run: `./init --plat pc99 --tut mapping --solution`

  File's location will be `./mapping/src/main.c`

In the beginning of the file, the `alloc_object` function is invoked. It is a
helper function which basically takes a free CSlot from the root's CSpace and
then calls `SeL4_Untyped_Retype` on the first available Untyped memory region
that is not a device in order to allocate a Kernel Object in that region. The
CSlot then becomes a capability to that Object. Finally that capability is
returned by `alloc_object`. Objects such as Page-directories or page-tables can
still be mapped on DRAM, so `alloc_object` is sufficient.  However, the frame
object needs to be mapped to the BRAM. To accomplish that, we create a similar
function to `alloc_object` which we call `alloc_objectIO` and add a few
definitions. The `alloc_object` function is defined in the
`projects/sel4-tutorials/libsel4tutorials/src/alloc.c` file. So we copy that
into our main file along with the `alloc_slot` function that is called inside
`alloc_object`.

We then create the additional `alloc_objectIO` function which is called once to
map the frame to the BRAM. It performs multiple calls to `SeL4_Untyped_Retype`
unti it reaches the exact address where the BRAM starts.  Furthermore, the
objects allocated in the tutorial are x86_64-specific. We change them for the
equivalent Aarch64.

After we allocate the objects, we need to map them to our virtual address
space. Again the mapping functions are x86_64 specific. We swap them with the
equivalent for Aarch64.


