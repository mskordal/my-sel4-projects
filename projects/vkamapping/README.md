# Mapping project using vka (Map BRAM to SeL4)
This is a tutorial similar to the
[Mapping](https://github.com/mskordal/my-sel4-projects/tree/main/projects/mapping)
but instead of manually allocating the page tables we use the VKA interface.
Also this project runs two applications instead of one.

# Prerequisites
Make sure you have understood the [Hello
World](https://github.com/mskordal/my-sel4-projects/tree/main/projects/hello)
and
[Mapping](https://github.com/mskordal/my-sel4-projects/tree/main/projects/mapping)
projects of the same repository.

In addition to the repositories that need to be downloaded in the `projects`
directory, this project requires the following cloning as well:
```bash
git clone https://github.com/seL4/sel4runtime.git projects/sel4runtime
```

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
To understand how to use the VKA and allocman libraries to automatically instantiate VSpaces, CSpaces, threads, process and map pages, Complete the following three tutorials

- Dynamic Libraries 1: <https://docs.sel4.systems/Tutorials/dynamic-1.html>

- Dynamic Libraries 2: <https://docs.sel4.systems/Tutorials/dynamic-2.html>

- Dynamic Libraries 3: <https://docs.sel4.systems/Tutorials/dynamic-3.html>

# Assumptions
We assume a BRAM in our hardware design starting at physical address 0xA0010000
with 8KB of size.

The SeL4 microkernel passes the root thread an array of capabilities to
available unmapped memory. Unmapped Memory is initially categorized into:
- **Untyped** which is DRAM unmapped memory

- **Device Untyped** which are regions corresponding to potentially added devices in
  our system which are currently unmapped.

# Strategy
In the
[Mapping](https://github.com/mskordal/my-sel4-projects/tree/main/projects/mapping)
tutorial, we had to manually allocate all objects and page structures to map a
frame in the BRAM. The VKA allows us to bypass several steps

Up to the point where we spawn a new process by calling
`sel4utils_spawn_process_v` our application is almost identical to the [Dynamic
Libraries 3](https://docs.sel4.systems/Tutorials/dynamic-3.html) tutorial. We
then use parts of [Dynamic Libraries
2](https://docs.sel4.systems/Tutorials/dynamic-2.html) to map BRAM. More
specifically:

1. We call `vka_alloc_object_at_maybe_dev` instead of `vka_alloc_frame` which
   is called 2 levels deeper than the latter. This function allocates a
   capability to the frame that we will later map in the bram. It allows to
   specify the exact physical we want to map from plus specify that what we
   want to map is a device. We no longer need to map intermediate objects. We
   can immidiately specify the exact base address of the BRAM.

2. Next, we allocate the object of the last level PTE of our frame using
   `vka_alloc_page_table`.

3. We then map the two objects, first the PTE using `seL4_ARCH_PageTable_Map`
   and then the frame using `seL4_ARCH_Page_Map`. In both cases we use the
   hardcoded virtual address `0x7000000`. We don't know why, but for some
   reason all virtual addresses above `0x10000000` do not work.

# Files
**src/main.c**: As explained earlier, our main file is a mixture of the two
main files of the Dynamic Libraries 2 and 3 tutorials.

**src/app.c**: This is a very simple c file in which we just call 3 functions
in a row.

**CMakeLists.txt**: Compared to  [Mapping
CMakeLists.txt](https://github.com/mskordal/my-sel4-projects/tree/main/projects/mapping/CMakeLists.txt),
we have to add an executable and target required link libraries for the second
file as well. We also use the `cpio` library to combine the two executables
into a single image.

**easy-settings.cmake** & **settings.cmake**: Those files are identical to
[Mapping](https://github.com/mskordal/my-sel4-projects/tree/main/projects/mapping).
