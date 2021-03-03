#!/usr/bin/python3

""" global settings """

import sys
import json
import os

__path = os.path.expanduser("~/.ubx_automation/settings.json")
__useDefaultSettings = True
__storeSettings = True
__settings = {}
if os.path.isfile(__path):
    __storeSettings = False
    try:
        with open(__path) as f:
            __settings = json.load(f)
        __useDefaultSettings = False
    except:
        print("u_settings: WARNING: settings file {} seems corrupt".format(__path))
else:
    print("u_settings: no settings at {}".format(__path))

__defaultSettings = {}
# u_connection.py
__defaultSettings["CONNECTION_INSTANCE_10"] = \
    {"serial_port":"COM10"}
__defaultSettings["CONNECTION_INSTANCE_11"] = \
    {"serial_port":"COM8"}
__defaultSettings["CONNECTION_INSTANCE_12"] = \
    {"serial_port":"COM9"}
__defaultSettings["CONNECTION_INSTANCE_13"] = \
    {"serial_port":"COM6", "debugger":"683253856"}
__defaultSettings["CONNECTION_INSTANCE_14"] = \
    {"serial_port":"COM5", "debugger":"066EFF515551678367072825"}
__defaultSettings["CONNECTION_INSTANCE_15"] = \
    {"serial_port":"COM7", "debugger":"683920969"}
__defaultSettings["CONNECTION_INSTANCE_16"] = \
    {"serial_port":"COM4", "debugger":"066FFF565053787567193329"}
__defaultSettings["CONNECTION_INSTANCE_17"] = \
    {"serial_port": "COM15", "debugger": "960115898"}
__defaultSettings["CONNECTION_INSTANCE_18"] = \
    {"serial_port": "COM22", "debugger": "960107696"}
__defaultSettings["CONNECTION_INSTANCE_19"] = \
    {"serial_port": "COM20", "debugger": "0672FF565053787567161452"}

# u_data.py
__defaultSettings["DATA_FILE"] = "DATABASE.md"
__defaultSettings["CELLULAR_MODULE_TYPE_PREFIX"] = "U_CELL_MODULE_TYPE_"
__defaultSettings["SHORT_RANGE_MODULE_TYPE_PREFIX"] = "U_SHORT_RANGE_MODULE_TYPE_"
# u_monitor.py
# u_pull_request.py
__defaultSettings["INSTANCE_DIR_PREFIX"] = "u_instance_"
__defaultSettings["STILL_RUNNING_REPORT_SECONDS"] = 30
# u_report.py
# u_run_astyle.py
__defaultSettings["ASTYLE_CONFIG_FILE"] = "astyle.cfg"
__defaultSettings["ASTYLE_FILE_EXTENSIONS"] = "*.c,*.cpp,*.h,*.hpp"
__defaultSettings["ASTYLE_DIRS"] = ["cfg", "port", "common"]
__defaultSettings["ASTYLE_EXCLUDE_DIRS"] = ["build", "_build", "Output", "Debug", "Release", "custom_boards"]
# u_run_doxygen.py
__defaultSettings["DOXYGEN_DOXYFILE"] = "Doxyfile"
#u_run_static_size.py
__defaultSettings["STATIC_SIZE_ARM_GNU_INSTALL_ROOT"] = "C:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2020-q4-major/bin"
__defaultSettings["STATIC_SIZE_C_FLAGS"] = "-Os -g0 -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16"
__defaultSettings["STATIC_SIZE_LD_FLAGS"] = "-Os -g0 -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16 --specs=nano.specs -lc -lnosys -lm"
__defaultSettings["STATIC_SIZE_LD_FLAGS_SUB_DIR"] = "port/platform/static_size"
# u_run_esp32.py
__defaultSettings["ESP_IDF_ROOT"] = "c:\\esp32"
__defaultSettings["ESP_IDF_TOOLS_PATH"] =                                          \
    __defaultSettings["ESP_IDF_ROOT"] + os.sep + "esp-idf-tools-latest"
__defaultSettings["ESP_IDF_LOCATION_LATEST"] =                                     \
    {"url": "https://github.com/espressif/esp-idf",
        "subdir": "esp-idf-latest",
        "branch": "release/v4.1"}
__defaultSettings["ESP_IDF_LOCATION_NINA_W1"] =                                    \
    {"url": "https://github.com/u-blox/esp-idf",
        "subdir": "esp-idf-u-blox",
        "branch": "master"}
__defaultSettings["ESP_IDF_PROJECT_SUBDIR"] = "runner"
__defaultSettings["ESP_IDF_TEST_COMPONENT"] = "ubxlib_runner"
__defaultSettings["ESP_IDF_BUILD_SUBDIR"] = "build"
__defaultSettings["ESP_IDF_INSTALL_GUARD_TIME_SECONDS"] = 60 * 60
__defaultSettings["ESP_IDF_BUILD_GUARD_TIME_SECONDS"] = 60 * 30
__defaultSettings["ESP_IDF_DOWNLOAD_GUARD_TIME_SECONDS"] = 60 * 5
# u_run_lint.py
__defaultSettings["LINT_PLATFORM_CONFIG_FILES"] = ["co-gcc.lnt", "ubxlib.lnt"]
__defaultSettings["LINT_COMPILER_INCLUDE_DIRS"] =                                  \
    ["C:\\TDM-GCC-64\\x86_64-w64-mingw32\\include",
        "C:\\TDM-GCC-64\\lib\\gcc\\x86_64-w64-mingw32\\9.2.0\\include"]
# u_run_nrf5sdk.py
__defaultSettings["NRF5SDK_GNU_INSTALL_ROOT"] =                                      \
    "C:/Program Files (x86)/GNU Arm Embedded Toolchain/10 2020-q4-major/bin/"
__defaultSettings["NRF5SDK_GNU_PREFIX"] = "arm-none-eabi"
__defaultSettings["NRF5SDK_GNU_VERSION"] = "10.2.1"
__defaultSettings["NRF5SDK_SES_PATH"] =                                              \
    "C:\\Program Files\\Segger\\SEGGER Embedded Studio for ARM 4.52c\\bin"
__defaultSettings["NRF5SDK_SES_NAME"] = "embuild.exe"
__defaultSettings["NRF5SDK_SES_BUILD_CONFIGURATION"] = "Debug"
__defaultSettings["NRF5SDK_NRF5_PATH"] = "C:/nrf5"
__defaultSettings["NRF5SDK_NRF52_RUN_JLINK"] = ["-Device", "NRF52840_XXAA", "-If", "SWD",
            "-Speed", "4000", "-Autoconnect", "1", "-ExitOnError", "1"]
__defaultSettings["NRF5SDK_NRF52_RUNNER_DIR_GCC"] = "port/platform/nrf5sdk/mcu/nrf52/gcc/runner"
__defaultSettings["NRF5SDK_NRF52_RUNNER_DIR_SES"] = "port/platform/nrf5sdk/mcu/nrf52/ses/runner"
__defaultSettings["NRF5SDK_PROJECT_NAME_SES"] = "u_pca10056"
__defaultSettings["NRF5SDK_BUILD_SUBDIR_PREFIX_GCC"] = "build_"
__defaultSettings["NRF5SDK_BUILD_SUBDIR_SES"] = "Output"
__defaultSettings["NRF5SDK_SES_MAX_NUM_DEFINES"] = 20
__defaultSettings["NRF5SDK_BUILD_GUARD_TIME_SECONDS"] = 60 * 30
# u_run_zephyr.py
__defaultSettings["ZEPHYR_NRFCONNECT_PATH"] = "C:\\nrfconnect\\v1.4.2"
__defaultSettings["ZEPHYR_RUN_JLINK"] = ["-If", "SWD", "-Speed", "4000", "-Autoconnect",
            "1", "-ExitOnError", "1"]
__defaultSettings["ZEPHYR_ZEPHYR_ENV_CMD"] =                                        \
    __defaultSettings["ZEPHYR_NRFCONNECT_PATH"] + os.sep + "zephyr\\zephyr-env.cmd"
__defaultSettings["ZEPHYR_GIT_BASH_ENV_CMD"] =                                      \
    __defaultSettings["ZEPHYR_NRFCONNECT_PATH"] + os.sep + "toolchain\\cmd\\env.cmd"
__defaultSettings["ZEPHYR_DIR"] = "port\\platform\\zephyr"
__defaultSettings["ZEPHYR_CUSTOM_BOARD_DIR"] =                                      \
    __defaultSettings["ZEPHYR_DIR"] + os.sep + "custom_boards\\zephyr\\boards\\arm"
__defaultSettings["ZEPHYR_CUSTOM_BOARD_ROOT"] =                                     \
    __defaultSettings["ZEPHYR_DIR"] + os.sep + "custom_boards\\zephyr"
__defaultSettings["ZEPHYR_BUILD_SUBDIR"] = "build"
__defaultSettings["ZEPHYR_BUILD_GUARD_TIME_SECONDS"] = 60 * 30
# u_run_pylint.py
# u_run_stm32cube.py
__defaultSettings["STM32CUBE_STM32CUBE_FW_PATH"] = "C:\\STM32Cube_FW_F4"
__defaultSettings["STM32CUBE_STM32CUBE_IDE_PATH"] = "C:\\ST\\STM32CubeIDE_1.4.0\\STM32CubeIDE"
__defaultSettings["STM32CUBE_STM32_PROGRAMMER_CLI_PATH"] =                   \
    __defaultSettings["STM32CUBE_STM32CUBE_IDE_PATH"] + "\\plugins\\"        \
    + "com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer."               \
    + "win32_1.4.0.202007081208\\tools\\bin\\STM32_Programmer_CLI.exe"
__defaultSettings["STM32CUBE_OPENOCD_PATH"] =                                \
    __defaultSettings["STM32CUBE_STM32CUBE_IDE_PATH"] + "\\plugins\\"        \
    + "com.st.stm32cube.ide.mcu.externaltools.openocd."                      \
    + "win32_1.4.0.202007081208\\tools\\bin\\openocd.exe"
__defaultSettings["STM32CUBE_OPENOCD_SCRIPTS_PATH"] =                        \
    __defaultSettings["STM32CUBE_STM32CUBE_IDE_PATH"] + "\\plugins\\"        \
    + "com.st.stm32cube.ide.mcu.debug."                                      \
    + "openocd_1.4.0.202007081208\\resources\\openocd\\st_scripts"
__defaultSettings["STM32CUBE_OPENOCD_STLINK_INTERFACE_SCRIPT"] = "stlink.cfg"
__defaultSettings["STM32CUBE_OPENOCD_STM32F4_TARGET_SCRIPT"] = "stm32f4x.cfg"
__defaultSettings["STM32CUBE_IDE_WORKSPACE_SUBDIR"] = "workspace_1.4.0"
__defaultSettings["STM32CUBE_SDK_DIR"] = "port\\platform\\stm32cube"
__defaultSettings["STM32CUBE_PROJECT_NAME"] = "runner"
__defaultSettings["STM32CUBE_UPDATED_PROJECT_NAME_PREFIX"] = "test_only_"
__defaultSettings["STM32CUBE_PROJECT_CONFIGURATION"] = "Debug"
__defaultSettings["STM32CUBE_MAX_NUM_DEFINES"] = 20
__defaultSettings["STM32CUBE_SYSTEM_CORE_CLOCK_HZ"] = 168000000
__defaultSettings["STM32CUBE_SWO_CLOCK_HZ"] = 125000
__defaultSettings["STM32CUBE_SWO_DATA_FILE"] = "swo.dat"
__defaultSettings["STM32CUBE_SWO_DECODED_TEXT_FILE"] = "swo.txt"
__defaultSettings["STM32CUBE_BUILD_GUARD_TIME_SECONDS"] = 60 * 30
# u_run.py
__defaultSettings["BRANCH_DEFAULT"] = "origin/master"
# u_utils.py
__defaultSettings["INSTALL_LOCK_WAIT_SECONDS"] = 60*60
__defaultSettings["UNITY_URL"] = "https://github.com/ThrowTheSwitch/Unity"
__defaultSettings["UNITY_SUBDIR"] = "Unity"
__defaultSettings["DEVCON_PATH"] = "devcon"
__defaultSettings["JLINK_PATH"] = "jlink.exe"
__defaultSettings["JLINK_SWO_PORT"] = 19021
__defaultSettings["STLINK_GDB_PORT"] = 61200
__defaultSettings["STLINK_SWO_PORT"] = 61300
__defaultSettings["TIME_FORMAT"] = "%Y-%m-%d_%H:%M:%S"
__defaultSettings["PLATFORM_LOCK_GUARD_TIME_SECONDS"] = 60 * 60
__defaultSettings["DOWNLOAD_GUARD_TIME_SECONDS"] = 120
__defaultSettings["RUN_GUARD_TIME_SECONDS"] = 60 * 60
__defaultSettings["RUN_INACTIVITY_TIME_SECONDS"] = 60 * 5
__defaultSettings["FILTER_MACRO_NAME"] = "U_CFG_APP_FILTER"
__defaultSettings["EXE_RUN_QUEUE_WAIT_SECONDS"] = 1

if __useDefaultSettings:
    print("u_settings: using default settings")
    __settings = __defaultSettings
else:
    for setKey in __settings:
        __defaultSettings.pop(setKey, None)
    __storeSettings = len(__defaultSettings) > 0 # merged default values, so store settings again
    for defKey in __defaultSettings:
        __settings[defKey] = __defaultSettings[defKey]

# populate this module with settings
__current_module = sys.modules[__name__]
for __key in __settings:
    #print("{} = {}".format(__key, __settings[__key]))
    setattr(__current_module, __key, __settings[__key])

if __storeSettings:
    print("u_settings: creating settings file {}".format(__path))
    if not os.path.isdir(os.path.expanduser("~/.ubx_automation")):
        os.makedirs(os.path.expanduser("~/.ubx_automation"))
    with open(__path, 'w') as out:
        json.dump(__settings, out, indent=2)
