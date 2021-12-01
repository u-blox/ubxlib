# Introduction
This directory contains the build infrastructure for STM32F4 MCU using the STM32Cube IDE.  The STM32F4 device configuration matches that of the STM32F437VG device that is mounted on the u-blox C030 board and configures it sufficiently well to run the ubxlib tests and examples: no attempt is made to optimise the MCU RAM/flash etc. sizes, you need to know how to do that yourself.

# SDK Installation And Usage
Follow the instructions to install the STM32Cube IDE:

https://www.st.com/en/development-tools/stm32cubeide.html

Download a version (this code was tested with version 1.25.0) of the STM32F4 MCU package ZIP file (containing their HAL etc.) from here:

https://www.st.com/en/embedded-software/stm32cubef4.html

Unzip it to the same directory as you cloned this repo with the name `STM32Cube_FW_F4`, i.e.:

```
..
.
STM32Cube_FW_F4
ubxlib
```

You may unzip it to a different location but, if you do so, when you open your chosen build you must go to `Project` -> `Properties` -> `Resource` -> `Linked Resources`, modify the path variable `STM32CUBE_FW_PATH` to point to the correct location and then refresh the project.

You may override or provide conditional compilation flags to this build without modifying the build file.  To do this, create an environment variable called `U_FLAGx`, where `x` is a number from 0 to 29, e.g. `U_FLAG0`, and set it to your conditional compilation flag with a `-D` prefix, e.g.:

```
set U_FLAG0=-DU_CFG_APP_PIN_CELL_ENABLE_POWER=-1
set U_FLAG1=-DMY_FLAG
set U_FLAG2=-DHSE_VALUE=((uint32_t)8000000U)
```

With that done, follow the instructions in the relevant sub-directory (e.g. `runner`) to build/download/run the project.

# Viewing Trace Output
To view the SWO trace output in the STM32Cube IDE, setup up the debugger as normal by pulling down the arrow beside the little button on the toolbar with the "bug" image on it, selecting "STM-Cortex-M C/C++ Application", create a new configuration and then, on the "Debugger" tab, tick the boxes that enable SWD and SWV, set the core clock to 168 MHz, the SWO Clock to 125 kHz (to match `U_CFG_HW_SWO_CLOCK_HZ` defined in `u_cfg_hw_platform_specific.h`), untick "Wait for sync packet" and click "Apply".

# Chip Resource Requirements
The SysTick of the STM32F4 is assumed to provide a 1 ms RTOS tick which is used as a source of time for `uPortGetTickTimeMs()`.  Note that this means that if you want to use FreeRTOS in tickless mode you will need to either find another source of tick for `uPortGetTickTimeMs()` or put in a call that updates `gTickTimerRtosCount` when FreeRTOS resumes after a tickless period.

# Trace Output
In order to conserve HW resources the trace output from this platform is sent over SWD.  To view the SWO trace output in the STM32Cube IDE, setup up the debugger as normal by pulling down the arrow beside the little button on the toolbar with the "bug" image on it, selecting "STM-Cortex-M C/C++ Application", create a new configuration and then, on the "Debugger" tab, tick the box that enables SWD, set the core clock to 168 MHz, the SWO Clock to 125 kHz (to match `U_CFG_HW_SWO_CLOCK_HZ` defined in `u_cfg_hw_platform_specific.h`), untick "Wait for sync packet" and click "Apply".

You should then be able to download the Debug build from the IDE and the IDE should launch you into the debugger.  To see the SWD trace output, click on "Window" -> "Show View" -> "SWV" -> "SWV ITM Data Console".  The docked window that appears should have a little "spanner" icon on the far right: click on that icon and, on the set of "ITM Stimulus Ports", tick channel 0 and then press "OK".  Beside the "spanner" icon is a small red button: press that to allow trace output to appear; unfortunately it seems that this latter step has to be performed every debug session, ST have chosen not to automate it for some reason.

Alternatively, if you just want to run the target without the debugger and simply view the SWO output, the (ST-Link utility)[https://www.st.com/en/development-tools/stsw-link004.html] utility includes a "Printf via SWO Viewer" option under its "ST-LINK" menu.  Set the core clock to 168 MHz, press "Start" and your debug `printf()`s will appear in that window.  HOWEVER, in this tool ST have fixed the expected SWO clock at 2 MHz whereas in normal operation we run it at 125 kHz to improve reliability; to use the ST-Link viewer you must unfortunately set the conditional complation flag `U_CFG_HW_SWO_CLOCK_HZ` to 2000000 before you build the code and then hope that trace output is sufficiently reliable (for adhoc use it is usually fine, it is under constant automated test that the cracks start to appear).

# newlib workarounds

## stdio memory leak
The `newlib` supplied via STM32CubeIDE (v1.7.0) is built with the config `_REENT_SMALL` and `_LITE_EXIT`. This configuration will together with FreeRTOS `configUSE_NEWLIB_REENTRANT=1` create memory leaks if the `stdio` streams are used on a task that later on is deleted. The reasons for this are:
* When `_REENT_SMALL` is enabled reentrant related resources will be dynamically allocated when they are first used. I.e. the first time `printf()` is used for a task a large buffer will be dynamically allocated and at this moment the `stdout` stream will be opened.
* When `_LITE_EXIT` is enabled `newlib` will not close the `stdio` streams on cleanup. I.e. if `stdout` has been opened for a task it will be kept open even after the task has been deleted.
To mitigate this issue we manually close `stdout`, `stdin` and `stderr` in `uPortTaskDelete()` for the STM32 port. However, it important to note that this workaround only works when a task deletes itself (`uPortTaskDelete(NULL)`) as we cannot access the reent instance from another task.

## FILE pointer pool
Another effect from `_REENT_SMALL` is that `FILE` pointers (that among other things are used for the `stdio` streams) are stored in a global pool. When there are no free `FILE` pointer available in this pool `newlib` will dynamically allocate a new one. This is not a memory leak as such since the `FILE` pointers will be re-used when they are closed. However, for our test system that checks for memory leaks it becomes cumbersome since we don't know when these allocations will happen. For this reason we have added a workaround to `preambleHeapDefence()` that will pre-allocate a set of `FILE` pointers that should eliminate the need for `newlib` to do any further allocations during the tests.

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
    
    If your new API is not under `common`, instead it is under a new and distinct category of APIs, e.g. [wifi](/wifi), that you want to appear as a new and distinct folder in the STM32Cube IDE view then create a new `<name />` for it, e.g:
    
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
- When STM32CubeIDE is upgraded a new version of `newlib` may be included which may use a different build configuration. Since we have some workarounds for the STM `newlib` mentioned above they may need some attention.