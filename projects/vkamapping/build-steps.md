# Steps to build and run an SeL4 project

## vkamapping project on zcu102

### Build the Sel4 Project
```bash
# while on the Sel4 root directory

# Make sure you use the correct symbolic link 
ln -fs projects/vkamapping/easy-settings.cmake easy-settings.cmake

# Enter build directory.
cd sel4build
# Remove previous files if built for different project
rm -rf ./*

# Run the script to initialize build files
# DPLATFORM is set to ZCU102
# DTRIPLE is used when we ant to use clang instead of gcc
../init-build.sh -DPLATFORM=zcu102 -DTRIPLE=aarch64-linux-gnu

# Compile the SeL4 project
ninja
```

### Load hardware and debug
First turn the board on
```bash
# on another terminal open minicom to connect to the port of ZCU102
sudo minicom -D /dev/ttyUSB0

# On another terminal in the root directory source vivado settings
source ~/.local/Xilinx/Vivado/2022.2/settings64.sh
# Open vivado, open project, open hardware manager
# Open target, autoconnect, program device
vivado

# On another terminal in the root directory source vivado settings
source ~/.local/Xilinx/Vivado/2022.2/settings64.sh
# Open xsct to start loading the scripts
xsct
# From within xsct run the script to load the sel4 project to the board
source init-board.tcl
```
As soon as the script loads the pmufw, refresh the device in vivado to enable
the ILA dubugger before the image loads on the board