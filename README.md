# my-sel4-projects
Different SeL4 standalone projects for different purposes

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
Below are the core repositories required for every project to run:
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

# Clone util_libs repo into projects/util_libs
git clone https://github.com/seL4/sel4runtime.git projects/sel4runtime

# seL4_tools contains an usefull script : 'init-build.sh'. Let's sym-link it in our root directory.
ln -s tools/cmake-tool/init-build.sh init-build.sh

```

Create a symbolic link of the `easy-settings.cmake` file from the project you
wish to build at the root directory:
```bash
# E.g. create a link from the hello-world project directory
ln -fs projects/hello/easy-settings.cmake easy-settings.cmake
```

# Build and Run the project
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
## Other platforms
```bash
# for ZCU102 Xilinx board. Does not support simulation (Aarch64)
-DPLATFORM=zcu102 

# for zynq7000 board. Supports simulation (Armv7 32bit)
-DPLATFORM=zynq7000 

# x86 32bit. Supports simulation
-DPLATFORM=ia32
```
