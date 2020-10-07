# Introduction
This directory contains the build infrastructure for STM32F4 using the STM32Cube IDE.

# SDK Installation And Usage
Follow the instructions to install the STM32Cube IDE:

https://www.st.com/en/development-tools/stm32cubeide.html

Download a version (this code was tested with version 1.25.0) of the STM32 F4 MCU package ZIP file (containing their HAL etc.) from here:

https://www.st.com/en/embedded-software/stm32cubef4.html

Unzip it to the same directory as you cloned this repo with the name `STM32Cube_FW_F4`, i.e.:

```
..
.
STM32Cube_FW_F4
ubxlib
```

You may unzip it to a different location but, if you do so, when you open your chosen build you must go to `Project` -> `Properties` -> `Resource` -> `Linked Resources`, modify the path variable `STM32CUBE_FW_PATH` to point to the correct location and then refresh the project.

You may override or provide conditional compilation flags to this build without modifying the build file.  To do this, create an environment variable called `U_FLAGx`, where `x` is a number from 0 to 19, e.g. `U_FLAG0`, and set it to your conditional compilation flag with a `-D` prefix, e.g.:

```
set U_FLAG0=-DU_CFG_APP_PIN_CELL_ENABLE_POWER=-1
set U_FLAG1=-DMY_FLAG
set U_FLAG2=-DHSE_VALUE=((uint32_t)8000000U)
```

With that done, follow the instructions in the relevant sub-directory (e.g. `runner`) to build/download/run the project.

# Viewing Trace Output
To view the SWO trace output in the STM32Cube IDE, setup up the debugger as normal by pulling down the arrow beside the little button on the toolbar with the "bug" image on it, selecting "STM-Cortex-M C/C++ Application", create a new configuration and then, on the "Debugger" tab, tick the box that enables SWD, set the core clock to 168 MHz, the SWO Clock to 250 kHz (to match `U_CFG_HW_SWO_CLOCK_HZ` defined in `u_cfg_hw_platform_specific.h`) and click "Apply".

You should then be able to download the Debug build from the IDE and the IDE should launch you into the debugger.  To see the SWD trace output, click on "Window" -> "Show View" -> "SWV" -> "SWV ITM Data Console".  The docked window that appears should have a little "spanner" icon on the far right: click on that icon and, on the set of "ITM Stimulus Ports", tick channel 0 and then press "OK".  Beside the "spanner" icon is a small red button: press that to allow trace output to appear; unfortunately it seems that this latter step has to be performed every debug session, it is not possible to automate it.

# Maintenance
- When updating this build to a new version of `STM32Cube_FW_F4` change the release version stated in the introduction above.
- When adding a new API (this is complicated, needing changes in multiple places in two files):
  - for each `.project` file in the directories below this:
    - for each `.c` file add a `<type>1</type>` `<link />` to the `<linkedResources>` section, e.g. if you were adding a new API in the `common` folder, for each `.c` file you would add something like:
    
    ```
    <link>
        <name>Ubxlib/U-Blox/Common/my_new_thing.c</name>
        <type>1</type>
        <locationURI>$%7BUBX_PATH%7D/common/my_new_thing/src/a_new_file.c</locationURI>
    </link>
    ```
    
    If your new API is not under `common`, instead it is under a new and distinct category of APIs, e.g. `wifi`, that you want to appear as a new and distinct folder in the STM32Cube IDE view then create a new `<name />` for it, e.g:
    
    ```
    <link>
        <name>Ubxlib/U-Blox/MyNewTypeOfThing/my_new_thing.c</name>
        <type>1</type>
        <locationURI>$%7BUBX_PATH%7D/my_new_folder/my_new_thing/src/a_new_file.c</locationURI>
    </link>
    ```
    
    - for each folder containing header files that need to be included, e.g. the `api` directories, add a `<type>2</type>` `<link />` in the same way, e.g. if you were adding a new API in the `common` folder you would add:.
    
    ```
    <link>
        <name>Ubxlib/Inc/U-Blox/common/my_new_thing/api</name>
        <type>2</type>
        <locationURI>$%7BUBX_PATH%7D/common/my_new_thing/api</locationURI>
    </link>
    ```
    
  - for each `.cproject` file in the directories below, find the `<listOptionValue />` entries (there are two, one for a release build and one for a debug build) and add a reference to the `<type>2</type>` (i.e. the include files) `<link />` you made above to **both** lists.  For instance, if in the `.project` file you added:
  
  ```
  <link>
      <name>Ubxlib/Inc/U-Blox/common/my_new_thing/api</name>
      <type>2</type>
      <locationURI>$%7BUBX_PATH%7D/common/my_new_thing/api</locationURI>
  </link>
  ```
  
  ...then in the `.cproject` file you would add:
  
  ```
  <listOptionValue builtIn="false" value="${workspace_loc:/${ProjName}/Ubxlib/Inc/U-Blox/common/my_new_thing/api}"/>
  ```
  
  ...i.e. the `<name/>` in the `.project` file must appear in the `value` field between `${workspace_loc:/${ProjName}/` and `}` in the new `<listOptionValue/>` line.
  
  When you have made and saved your changes, keep the modified files open in an editor (e.g. `notepad++`), start the STM32Cube IDE, load or refresh the project and check that everything looks right and that the code compiles.  The STM32Cube IDE has a tendency to *fiddle* with the contents of the `.project` file but can get that wrong and lose the entire `<linkedResources />` section from the `.project` file; keeping the modified files open in an editor should allow you to recover if that happens.