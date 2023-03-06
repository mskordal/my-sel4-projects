# Hello World project

This is a simple programme that prints "Hello World" when ran.

# Files
- **src/main.c**: The one and only source file
- **CMakeLists.txt**: Main cmake file.  Includes `settings.cmake`. Includes all
  required libraries and links the source files to create the final images.
  Also includes simulation if specified
- **settings.cmake**: Initialization instructions. Includes `easy-settings.cmake.`
  Sets important flags such as simulation. `init-build.sh` uses this file to
  load the initial settings.
- **easy-settings.cmake**: Sets flags in the Kernel and is useb by
  `init-build.sh` to determine the project's root directory.
