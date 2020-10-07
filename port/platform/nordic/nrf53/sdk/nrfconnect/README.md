# Introduction
This directory and its sub-directories contain the build infrastructure for Nordic NRF5340 building using NRFConnect SDK.
The method described here is using Segger Embedded Studio (SES) but will also work using command line described in Nordic documents.
- This is tested on `nRFConnect SDK version 1.3.0` which is the recommended version.

# SDK Installation
Follow the instructions to install the development tools:

- Install nRF connect. https://www.nordicsemi.com/Software-and-Tools/Development-Tools/nRF-Connect-for-desktop
- Start nrf connect and use the tool chain manager to install the recommended SDK version (see above).
- IMPORTANT!: Update SDK and toolchain using the dropdown menu for your SDK version.
From tool chain manager start Segger embedded studio (SES) using Open IDE button.


# SDK Usage

- Always load project from SES using file->Open nRF connect SDK project
- Select CMakeLists.txt of the application you want to build.
- Board file should be "{your_sdk_path}/zephyr/boards/arm/nrf5340pdk_nrf5340"
- Board name should be "nrf5340pdk_nrf5340_cpuapp"
- Always use the clean build directory option when upgrading to new ubxlib version!
- You may override or provide conditional compilation flags to CMake without modifying `CMakeLists.txt`.  Do this by setting an environment variable `U_FLAGS`, e.g.:
  
  ```
  set U_FLAGS=-DMY_FLAG
  ```
  
  ...or:
  
  ```
  set U_FLAGS=-DMY_FLAG -DU_CFG_APP_PIN_CELL_ENABLE_POWER=-1
  ```

# Maintenance
- When updating this build to a new version of the NRFConnect SDK change the release version stated in the introduction above.