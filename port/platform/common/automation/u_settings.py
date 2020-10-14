#!/usr/bin/python3

""" global settings """

import sys
import json
import os

__path = os.path.expanduser("~/.ubx_automation/settings.json")
__defSettings = True
__createSettings = True
__settings = {}
if os.path.isfile(__path):
    __createSettings = False
    try:
        with open(__path) as f:
            __settings = json.load(f)
        __defSettings = False
    except:
        print("u_settings: WARNING: settings file {} seems corrupt".format(__path))
else:
    print("u_settings: no settings at {}".format(__path))

if __defSettings:
    print("u_settings: using default settings")
    # u_data.py
    __settings["DATA_FILE"] = "DATABASE.md"
    __settings["CELLULAR_MODULE_TYPE_PREFIX"] = "U_CELL_MODULE_TYPE_"
    # u_monitor.py
    # u_pull_request.py
    __settings["INSTANCE_DIR_PREFIX"] = "u_instance_"
    __settings["STILL_RUNNING_REPORT_SECONDS"] = 30
    # u_report.py
    # u_run_astyle.py
    __settings["ASTYLE_CONFIG_FILE"] = "astyle.cfg"
    __settings["ASTYLE_FILE_EXTENSIONS"] = "*.c,*.cpp,*.h,*.hpp"
    __settings["ASTYLE_DIRS"] = ["cfg", "port", "common"]
    __settings["ASTYLE_EXCLUDE_DIRS"] = ["build", "_build", "Output", "Debug", "Release"]
    # u_run_doxygen.py
    __settings["DOXYGEN_DOXYFILE"] = "Doxyfile"
    # u_run_esp32.py
    __settings["ESP_IDF_ROOT"] = "c:\\esp32"
    __settings["ESP_IDF_TOOLS_PATH"] =                                          \
        __settings["ESP_IDF_ROOT"] + os.sep + "esp-idf-tools-latest"
    __settings["ESP_IDF_LOCATION_LATEST"] =                                     \
        {"url": "https://github.com/espressif/esp-idf",
         "subdir": "esp-idf-latest",
         "branch": "release/v4.1"}
    __settings["ESP_IDF_LOCATION_NINA_W1"] =                                    \
        {"url": "https://github.com/u-blox/esp-idf",
         "subdir": "esp-idf-u-blox",
         "branch": "master"}
    __settings["ESP_PROJECT_SUBDIR"] = "runner"
    __settings["ESP_TEST_COMPONENT"] = "ubxlib_runner"
    __settings["ESP_BUILD_SUBDIR"] = "build"
    __settings["ESP_INSTALL_GUARD_TIME_SECONDS"] = 60 * 60
    __settings["ESP_BUILD_GUARD_TIME_SECONDS"] = 60 * 30
    __settings["ESP_DOWNLOAD_GUARD_TIME_SECONDS"] = 60 * 5
    # u_run_lint.py
    __settings["LINT_PLATFORM_CONFIG_FILES"] = ["co-gcc.lnt", "ubxlib.lnt"]
    __settings["LINT_COMPILER_INCLUDE_DIRS"] =                                  \
        ["C:\\TDM-GCC-64\\x86_64-w64-mingw32\\include",
         "C:\\TDM-GCC-64\\lib\\gcc\\x86_64-w64-mingw32\\9.2.0\\include"]
    # u_run_nrf52.py
    __settings["NRF52_GNU_INSTALL_ROOT"] =                                      \
        "C:/Program Files (x86)/GNU Tools ARM Embedded/9 2019-q4-major/bin/"
    __settings["NRF52_GNU_PREFIX"] = "arm-none-eabi"
    __settings["NRF52_GNU_VERSION"] = "9.2.1"
    __settings["NRF52_SES_PATH"] =                                              \
        "C:\\Program Files\\Segger\\SEGGER Embedded Studio for ARM 4.52c\\bin"
    __settings["NRF52_SES_NAME"] = "embuild.exe"
    __settings["NRF52_SES_BUILD_CONFIGURATION"] = "Debug"
    __settings["NRF52_NRF5_PATH"] = "C:/nrf5"
    __settings["NRF52_RUN_JLINK"] = ["-Device", "NRF52840_XXAA", "-If", "SWD",
             "-Speed", "4000", "-Autoconnect", "1", "-ExitOnError", "1"]
    __settings["NRF52_RUNNER_DIR_GCC"] = "port/platform/nordic/nrf52/sdk/gcc/runner"
    __settings["NRF52_RUNNER_DIR_SES"] = "port/platform/nordic/nrf52/sdk/ses/runner"
    __settings["NRF52_PROJECT_NAME_SES"] = "u_pca10056"
    __settings["NRF52_BUILD_SUBDIR_PREFIX_GCC"] = "build_"
    __settings["NRF52_BUILD_SUBDIR_SES"] = "Output"
    __settings["NRF52_SES_MAX_NUM_DEFINES"] = 20
    __settings["NRF52_BUILD_GUARD_TIME_SECONDS"] = 60 * 30
    # u_run_nrf53.py
    __settings["NRF53_NRFCONNECT_PATH"] = "C:\\nrfconnect\\v1.3.0"
    __settings["NRF53_ZEPHYR_ENV_CMD"] =                                        \
        __settings["NRF53_NRFCONNECT_PATH"] + os.sep + "zephyr\\zephyr-env.cmd"
    __settings["NRF53_GIT_BASH_ENV_CMD"] =                                      \
        __settings["NRF53_NRFCONNECT_PATH"] + os.sep + "toolchain\\cmd\\env.cmd"
    __settings["NRF53_RUN_JLINK"] = ["-Device", "nRF5340_xxAA_APP", "-If", "SWD",
             "-Speed", "4000", "-Autoconnect", "1", "-ExitOnError", "1"]
    __settings["NRF53_RUNNER_DIR"] = "port\\platform\\nordic\\nrf53\\sdk\\nrfconnect\\runner"
    __settings["NRF53_BOARD_NAME"] = "nrf5340pdk_nrf5340_cpuapp"
    __settings["NRF53_BUILD_SUBDIR"] = "build"
    __settings["NRF53_BUILD_GUARD_TIME_SECONDS"] = 60 * 30
    # u_run_pylint.py
    # u_run_stm32f4.py
    __settings["STM32_STM32CUBE_FW_PATH"] = "C:\\STM32Cube_FW_F4"
    __settings["STM32_STM32CUBE_IDE_PATH"] = "C:\\ST\\STM32CubeIDE_1.3.0\\STM32CubeIDE"
    __settings["STM32_STM32_PROGRAMMER_BIN_PATH"] =                             \
        __settings["STM32_STM32CUBE_IDE_PATH"] + "\\plugins\\"                  \
        + "com.st.stm32cube.ide.mcu.externaltools.cubeprogrammer."              \
        + "win32_1.3.0.202002181050\\tools\\bin"
    __settings["STM32_STM32_PROGRAMMER_CLI_PATH"] =                             \
        __settings["STM32_STM32_PROGRAMMER_BIN_PATH"] + "\\STM32_Programmer_CLI.exe"
    __settings["STM32_STLINK_GDB_SERVER_PATH"] =                                \
        __settings["STM32_STM32CUBE_IDE_PATH"] + "\\plugins\\"                  \
        + "com.st.stm32cube.ide.mcu.externaltools.stlink-gdb-server."           \
        + "win32_1.3.0.202002181050\\tools\\bin\\ST-LINK_gdbserver.exe"
    __settings["STM32_STM32_GNU_ARM_GDB_SERVER_PATH"] =                         \
        __settings["STM32_STM32CUBE_IDE_PATH"] + "\\plugins\\"                  \
        + "com.st.stm32cube.ide.mcu.externaltools."                             \
        + "gnu-tools-for-stm32.7-2018-q2-update.win32_1.0.0.201904181610\\"     \
        + "tools\\bin\\arm-none-eabi-gdb.exe"
    __settings["STM32_STM32CUBE_IDE_WORKSPACE_SUBDIR"] = "workspace_1.3.0"
    __settings["STM32_SDK_DIR"] = "port\\platform\\stm\\stm32f4\\sdk\\cube"
    __settings["STM32_PROJECT_NAME"] = "runner"
    __settings["STM32_UPDATED_PROJECT_NAME_PREFIX"] = "test_only_"
    __settings["STM32_PROJECT_CONFIGURATION"] = "Debug"
    __settings["STM32_MAX_NUM_DEFINES"] = 20
    __settings["STM32_SYSTEM_CORE_CLOCK_HZ"] = 168000000
    __settings["STM32_SWO_CLOCK_HZ"] = 2000000
    __settings["STM32_STLINK_GDB_PORT"] = 61234
    __settings["STM32_STLINK_SWO_PORT"] = 61235
    __settings["STM32_SWO_DECODED_TEXT_FILE"] = "swo.txt"
    __settings["STM32_BUILD_GUARD_TIME_SECONDS"] = 60 * 30
    # u_run.py
    __settings["BRANCH_DEFAULT"] = "origin/master"
    # u_utils.py
    __settings["INSTALL_LOCK_WAIT_SECONDS"] = 60*60
    __settings["UNITY_URL"] = "https://github.com/ThrowTheSwitch/Unity"
    __settings["UNITY_SUBDIR"] = "Unity"
    __settings["JLINK_PATH"] = "jlink.exe"
    __settings["JLINK_SWO_PORT"] = 19021
    __settings["STLINK_GDB_PORT"] = 61200
    __settings["STLINK_SWO_PORT"] = 61300
    __settings["TIME_FORMAT"] = "%Y-%m-%d_%H:%M:%S"
    __settings["PLATFORM_LOCK_GUARD_TIME_SECONDS"] = 60 * 60
    __settings["DOWNLOAD_GUARD_TIME_SECONDS"] = 60
    __settings["RUN_GUARD_TIME_SECONDS"] = 60 * 60
    __settings["RUN_INACTIVITY_TIME_SECONDS"] = 60 * 5
    __settings["FILTER_MACRO_NAME"] = "U_CFG_APP_FILTER"
    __settings["EXE_RUN_QUEUE_WAIT_SECONDS"] = 1


# populate this module with settings
__current_module = sys.modules[__name__]
for __key in __settings:
    #print("{} = {}".format(__key, __settings[__key]))
    setattr(__current_module, __key, __settings[__key])

if __createSettings:
    print("u_settings: creating settings file {}".format(__path))
    os.makedirs(os.path.expanduser("~/.ubx_automation"))
    with open(__path, 'w+') as out:
        json.dump(__settings, out, indent=2)
