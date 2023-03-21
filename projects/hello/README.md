# Hello project (print hello world to SeL4)

This article is about the Sel4 microkernel. For additional information visit:
- <https://sel4.systems/>
- <https://docs.sel4.systems/>

This article shows the steps required to build and configure a basic SeL4
project by assembling all the required components from the official SeL4.
Current SeL4 documentation includes several projects and tutorials that are
built with automated scripts. The purpose of this article is to guide a user to
build a minimal SeL4 project skeleton, in which they can later add only the
components and code they require and build for their desired architecture.

# Documentation and Guides followed

There is already a repository that gives a definitive guide to build an SeL4
project from scratch:
<https://github.com/manu88/SeL4_101>

Since the latest update of this guide, there have been several changes to the
SeL4 Documenation and the composition of the build structure. This latest guide
focuses on tackling the latest file dependencies that are required for a
successful build.

The following pages in the SeL4 documentation were also read in combination
with the guide above:
- <https://docs.sel4.systems/projects/buildsystem/incorporating.html>
- <https://docs.sel4.systems/projects/buildsystem/using.html>

# Initial project structure

The SeL4 following components are required for the initial project structure:
- The SeL4 microkernel source code:
  <https://github.com/seL4/seL4>

- Tools used to build and configure SeL4 projects:
  <https://github.com/seL4/seL4_tools>

- A collection of libraries for working on seL4:
  <https://github.com/seL4/seL4_libs>

- Collection of OS independent utility libs:
  <https://github.com/seL4/util_libs>

- A minimal runtime for running a C or C-compatible processes:
  <https://github.com/seL4/sel4runtime>

A valid example of an initial skeleton of an SeL4 project is the following:
```
sel4HelloFromScratch/
├── kernel/
├── projects/
│   ├── hello/
│   ├── musllibc/
│   ├── seL4_libs/
│   ├── sel4runtime/
│   └── util_libs/
└── tools/
    ├── cmake-tool/
```

**kernel**: The directory contains the SeL4 microkernel source code.

**projects**: The directory contains all the libraries and utilities which are
required by application. Additionally it contains the root directory of any
user application is built for SeL4. In this case I named our application
'hello'.

**tools**: The directory contains the SeL4 tools to build and configure an SeL4 project.

# Downloading, Building and running the project

```bash
# SeL4 must be inside a directory named 'kernel'
git clone https://github.com/seL4/seL4.git kernel

# The SeL4 tools must be inside a directroy named 'tools'
git clone https://github.com/seL4/seL4_tools.git tools

# Create 'projects' folder
mkdir projects

# Clone seL4_libs repo into projects/seL4_libs
git clone https://github.com/seL4/seL4_libs.git projects/seL4_libs

# Clone the musl libc into projects/musllibc
git clone https://github.com/seL4/musllibc.git projects/musllibc

# Clone util_libs repo into projects/util_libs
git clone https://github.com/seL4/util_libs.git projects/util_libs

# seL4_tools contains an usefull script : 'init-build.sh'. Let's sym-link it in our root directory.
ln -s tools/cmake-tool/init-build.sh init-build.sh

```
All files required for the project are put into the `hello` directory. First of
all lets create a simple source file that just prints the "Hello World"
message. File is located in the
[projects/hello/src/](https://github.com/mskordal/my-sel4-projects/tree/main/projects/hello/src)
path.

Projects in SeL4 are built using the CMake tool. This requires a CMakeLists.txt
file in the project directory. Additionally, projects derived from the
tutorials or predefined projects like sel4test use two more files:

- **easy-settings.cmake**: This file sets several flags in the Kernel and is used
  by the latest `init-build.sh` file to determin the project's root directory.
  We keep that file as is.

- **settings.cmake**: This file contains initialization instructions  such us
  appending the kernel path, cmake-tool and elfloader tool for building the
  project. It includes `easy-settings.cmake` and among others, it sets
  important flags such as simulation flags if we decide to build a simulation
  project. Finally `init-build.sh` uses this file to load the the
  initial settings. We copy that directly from sel4test. We change
  lines 15 and 16 to match the paths to our `cmake-tool` directory, and we
  also remove lines 19 and 20 that set `NANOPB_SRC_ROOT_FOLDER` and
  `OPENSBI_PATH` since we do not need them for the hello world project.

For `CMakeLists.txt` we make a somewhat hybrid version:

- First, we build the [hello
  world](https://docs.sel4.systems/Tutorials/hello-world.html) tutorial of SeL4
  since it is kind of the same project but uses the build structure of the
  tutorials rather than a standalone project and those two differ at several
  points. We copy `CMakeLists.txt` from the tutorial and we apply the following
  changes:

	1. We remove everything from line 41 till the end since those settings are
	   tutorial-specific.

	2. In the `target_link_libraries` instruction, we only keep the project
	   name and libraries `sel4muslcsys` and `muslc`, since only those are
	   required for our project

- Second, we build the [sel4test](https://docs.sel4.systems/projects/sel4test/)
  project and copy the following parts of its `CMakeLists.txt`:

	1. We copy `include(settings.cmake)` since it is required in standalone
	   projects.

	2. We include the whole `if(SIMULATION)` statement at the end, which will
	   allow us to produce projects that can run in QEMU.
 
We put all three files at the
[projects/hello/](https://github.com/mskordal/my-sel4-projects/tree/main/projects/hello)
path since that's where they are looked for. The last thing we need to do is to
create a symbolic link to `easy-settings.cmake` in the root directory, since
`init-build.sh` requires it:
```bash
ln -s projects/hello/easy-settings.cmake easy-settings.cmake
```
Finally to build and run the project, we run the following:
```bash
# create a build directory and enter
mkdir build && cd build

# run the initialization script for x86 64bit and make it a QEMU simulation
../init-build.sh  -DPLATFORM=x86_64 -DSIMULATION=TRUE

# build the project
ninja

# run the build generated script that invokes QEMU
./simulate
```
You should see the "Hello World!" message followed by a printf warning and
a capability fault. You can exit QEMU with `Ctrl-a + x`.

Additional useful flags for `init-build.sh`:
- `-DTRIPLE=aarch64-linux-gnu`: DTRIPLE allows to use clang instead of gcc.
  aarch64-linux-gnu specifies that we want to compile for ARM 64bit
