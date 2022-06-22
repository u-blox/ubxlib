# Introduction
These directories provide the configuration and build metadata for Windows, sufficient to run the `ubxlib` tests and examples, talking to a u-blox device attached to the PC through a COM port.

- [cfg](cfg): contains the configuration files, for the application and for testing (mostly which ports are connected to which module(s)).
- [runner](runner): a build which runs all of the examples and unit tests.

# SDK Installation 
You will need Microsoft Visual C++ (this code was tested with version 19.30.30706) which must be on your path; it can be downloaded from here:

https://aka.ms/vs/17/release/vs_BuildTools.exe

... and, in the installation tool which runs, select to install "Desktop development with C++".  Microsoft Visual Studio comes with its own copy of CMake, which is also required, or it can be downloaded separately from here:

https://cmake.org/download/

To perform visual debugging install Visual Studio Code, which can be downloaded from here:

https://code.visualstudio.com/

...and, in Visual Studio Code, install the extensions `CMake Tools` and `C/C++ for Visual Studio Code`.

# IMPORTANT Note About char Types
In Microsoft Visual C++ `char` types are signed, which can lead to unexpected behaviours e.g. a character value which contains 0xaa, when compared with the literal value 0xaa, will return false; the character value is interpreted as being negative because it has the top bit set, while the literal value 0xaa is positive.  To avoid this problem you should use the command-line switch [/J](https://docs.microsoft.com/en-us/cpp/build/reference/j-default-char-type-is-unsigned) with the compiler.

This is done automatically for the `runner` build.

# SDK Usage
Before you can compile code under MSVC you need to invoke a specific command enviroment.  To do this, open a fresh command prompt and run a command of the following form:

`"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"`

...modifying the drive/year to match your particular installation of MSVC.

You may override or provide conditional compilation flags without modifying the build file.  Do this by adding a `U_FLAGS` environment variable, e.g.:

`U_FLAGS=-DU_CFG_APP_CELL_UART=3 -DU_CFG_TEST_CELL_MODULE_TYPE=U_CELL_MODULE_TYPE_SARA_R5`

Note: MSVC uses `#` as a way of passing `=` in the value of such a conditional compilation flag, so the first `#` that appears in the value of a flag will come out as `=` in the code, i.e. `-DTHING=1234#` will appear to the code as `#define THING 1234=`.  If the value of one of your conditional compilation flags happens to include a `#` then replace the `=` with a hash also; then, for example `-DTHING#1234#` will appear to the code as `#define THING 1234#`.

You may now build the code at that command prompt; create a build directory for yourself and, for instance, to build the `runner` build, you would enter `cmake -G <generator> <path to the runner directory>` to create the build metadata with the `<generator>` of your choice (but see the section that follows for the easiest way to proceed).

# Visual Studio Code
Visual Studio Code supports use of CMake directly: instead of running `cmake` from the command prompt as above, instead launch Visual Studio Code by entering `code`. Open your build folder, e.g. the `runner` directory below this, in Visual Studio Code, give it a moment and a prompt should pop-up in the bottom right-hand corner of the screen from the `CMake Tools` extension, asking if you want to configure the folder: click `Yes`.  Depending on how many compilers you have installed you may be given LOTS of options: select the **x86** version of the Microsoft Visual C++ toolchain you installed, e.g. the x86 version of Microsoft Visual Studio build tools 2022.

When this has run you should see your build targets in the left-hand window of the `CMake Tool` extension view; for instance in the case of the `runner` build you will see the `ubxlib_test_main` target.  Right-click on the `.exe` entry for that target to build it and then again to debug it.

Note that if you run your code under a debugger on Windows, unlike with an embedded platform, the timer tick is not paused when you pause the debugger.

# Maintenance
- When updating this build to a new version of the compiler, change the release version stated in the introduction above.