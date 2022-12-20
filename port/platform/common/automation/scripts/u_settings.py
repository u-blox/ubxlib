#!/usr/bin/python3

""" global settings """

import sys
import json
import os
import portalocker # To make sure that file accesses don't overlap

# *** START: IMPORTANT NOTE ABOUT "_AGENT_SPECIFIC" ***
#
# If you add a setting here that ends with "_AGENT_SPECIFIC"
# it will cause any test agents that do not have that setting
# to STOP DEAD until the relevant setting is added to their
# settings_v2_agent_specific.json files.
#
# So, please do NOT commit any code to ubxlib that adds an
# agent specific setting without FIRST adding the
# setting to settings_v2_agent_specific.json on all of
# the agents (for which you will likely need to know
# where they are and gain Remote Desktop access).
#
# *** END: IMPORTANT NOTE ABOUT "_AGENT_SPECIFIC" ***

# Use this post-fix on the name of a setting if
# the global configuration files MUST contain a
# value for it (with the post-fix removed) in
# order that any test scripts can be run. The
# post-fix is removed before the setting is
# employed.  If a default setting exists with
# this post-fix and there is no value in either
# of the global configuration files, this code
# will write the default value back to the
# agent specific global configuration file with
# the post-fix "_FIX_ME" on it, so that it is obvious
# which value needs attention; such post-fixed
# values will be ignored when the global
# configuration files are read. All the user
# then has to do is to edit the value(s) in the
# global configuration file(s) (if necessary)
# and remove the "_FIX_ME" post-fix.
__SETTINGS_POSTFIX_AGENT_SPECIFIC = "_AGENT_SPECIFIC"

# Value to append to an entry that is agent-specific,
# and hence should be define in the global
# configuration files, but isn't
__SETTINGS_POSTFIX_FIX_ME = "_FIX_ME"

# Use this post-fix on a setting while trying things
# out, e.g. on a branch, when you don't want the
# value to be written back to the global configuration
# files.  The post-fix is removed before the setting
# is employed.  If an entry with the part before the
# post-fix already exists in either the default
# settings or the settings read from file, the value
# from the post-fixed one will be used instead, hence
# this can be used to temporarily change existing
# values without affecting anyone else.
__SETTINGS_POSTFIX_TEST_ONLY_TEMP = "_TEST_ONLY_TEMP"

# The directory for the global configuration files
__SETTINGS_FILE_DIRECTORY = os.path.expanduser("~/.ubx_automation")

# The root name of the global configuration files
__SETTINGS_FILE_NAME_ROOT = "settings_v2"

# The root name of the previous version of the global configuration file
__SETTINGS_FILE_NAME_ROOT_OLD = "settings"

# The file extension of the global configuration files
__SETTINGS_FILE_NAME_EXT = "json"

# The path to the general global configuration file
__SETTINGS_FILE_PATH_GENERAL = __SETTINGS_FILE_DIRECTORY + os.sep + \
                               __SETTINGS_FILE_NAME_ROOT + "." +    \
                               __SETTINGS_FILE_NAME_EXT

# The path to the global configuration file that
# contains agent specific things
__SETTINGS_FILE_PATH_AGENT_SPECIFIC = __SETTINGS_FILE_DIRECTORY + os.sep +      \
                                      __SETTINGS_FILE_NAME_ROOT +               \
                                      __SETTINGS_POSTFIX_AGENT_SPECIFIC.lower()+\
                                      "." + __SETTINGS_FILE_NAME_EXT

# The path to the old global configuration file
__SETTINGS_FILE_PATH_OLD = __SETTINGS_FILE_DIRECTORY + os.sep + \
                           __SETTINGS_FILE_NAME_ROOT_OLD + "." +    \
                           __SETTINGS_FILE_NAME_EXT

# The version of these settings.  This should be
# incremented ONLY under the following circumstances:
#
# - if a default value has been modified and this value
#   MUST be updated in all of the global configuration
#   files for testing to continue, i.e. a breaking change,
# - if a new default value has been added which MUST be
#   overriden in the global configuration files, likely
#   one which has "_AGENT_SPECIFIC" on the end of it.
#
# Changing version will cause any test agents that
# have global configuration files of a different version
# to be paused until someone goes and sorts the
# differences out, so only increment the version with
# the agreement of people who have access to do that
# for ALL test agents or testing on those agents will
# stop.
#
# You do NOT need to increment the version when a new
# field is added that is NOT agent specific since the
# default values will be merged into the settings used
# in any case.
__SETTINGS_VERSION = 2

# The default settings
__DEFAULT_SETTINGS = {}
__DEFAULT_SETTINGS["SETTINGS_VERSION"] = __SETTINGS_VERSION
# u_agent.py
__DEFAULT_SETTINGS["USB_CUTTER_ID_STRS" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] = None
__DEFAULT_SETTINGS["HW_RESET_DURATION_SECONDS"] = 5
# u_connection.py
__DEFAULT_SETTINGS["CONNECTION_INSTANCE_10" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] = \
    {"serial_port": "/dev/ftdi_ft4232h"}
__DEFAULT_SETTINGS["CONNECTION_INSTANCE_11" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] = \
    {"serial_port": "/dev/silabs_cp210x_uart"}
__DEFAULT_SETTINGS["CONNECTION_INSTANCE_12" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] = \
    {"serial_port": "/dev/silabs_cp210x_uart"}
__DEFAULT_SETTINGS["CONNECTION_INSTANCE_13" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] = \
    {"serial_port": "COM6", "debugger":"683253856"}
__DEFAULT_SETTINGS["CONNECTION_INSTANCE_14" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] = \
    {"serial_port": "/dev/stlink_v2_1", "debugger":"066EFF515551678367072825"}
__DEFAULT_SETTINGS["CONNECTION_INSTANCE_15" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] = \
    {"serial_port": "/dev/segger_jlink_nrf52840", "debugger":"683920969"}
__DEFAULT_SETTINGS["CONNECTION_INSTANCE_16" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] = \
    {"serial_port": "COM3", "debugger":"066AFF565053787567193251"}
__DEFAULT_SETTINGS["CONNECTION_INSTANCE_17" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] = \
    {"serial_port": "/dev/segger_jlink_base", "debugger": "50102100"}
__DEFAULT_SETTINGS["CONNECTION_INSTANCE_18" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] = \
    {"serial_port": "/dev/segger_jlink_base", "debugger": "51014525"}
__DEFAULT_SETTINGS["CONNECTION_INSTANCE_19" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] = \
    {"serial_port": "/dev/stlink_v2_1", "debugger": "0672FF565053787567161452"}
__DEFAULT_SETTINGS["CONNECTION_INSTANCE_20" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] = \
    {"serial_port": "/dev/ftdi_ft4232h"}
__DEFAULT_SETTINGS["CONNECTION_INSTANCE_21" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] = \
    {"serial_port": "/dev/ftdi_ft4232h"}
__DEFAULT_SETTINGS["CONNECTION_INSTANCE_22" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] = \
    {"serial_port": "/dev/ftdi_ft4232h"}
# Don't need one for instance 23 [Windows] since there is nothing to program
# Don't need one for instance 24 [Zephyr/Linux] since there is nothing to program
__DEFAULT_SETTINGS["CONNECTION_INSTANCE_25" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] = \
    {"serial_port": "/dev/silabs_cp210x_uart"}
# u_data.py
__DEFAULT_SETTINGS["DATA_FILE"] = "DATABASE.md"
__DEFAULT_SETTINGS["CELLULAR_MODULE_TYPE_PREFIX"] = "U_CELL_MODULE_TYPE_"
__DEFAULT_SETTINGS["SHORT_RANGE_MODULE_TYPE_PREFIX"] = "U_SHORT_RANGE_MODULE_TYPE_"
__DEFAULT_SETTINGS["GNSS_MODULE_TYPE_PREFIX"] = "U_GNSS_MODULE_TYPE_"
# u_run_astyle.py
__DEFAULT_SETTINGS["ASTYLE_CONFIG_FILE"] = "astyle.cfg"
__DEFAULT_SETTINGS["ASTYLE_FILE_EXTENSIONS"] = "*.c,*.cpp,*.h,*.hpp"
__DEFAULT_SETTINGS["ASTYLE_DIRS"] = ["."]
# u_run_doxygen.py
__DEFAULT_SETTINGS["DOXYGEN_DOXYFILE"] = "Doxyfile"
#u_run_static_size.py
__DEFAULT_SETTINGS["STATIC_SIZE_ARM_GNU_INSTALL_ROOT" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] = \
    "/home/ubxlib/.ubxlibpkg/arm_embedded_gcc-10-2020-q4-major/bin/"
# u_run_windows.py
__DEFAULT_SETTINGS["WINDOWS_MSVC_BUILD_TOOLS_PATH" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] =         \
    "C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools"
__DEFAULT_SETTINGS["WINDOWS_MSVC_SETUP_BATCH_FILE" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] =         \
    __DEFAULT_SETTINGS["WINDOWS_MSVC_BUILD_TOOLS_PATH" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] +     \
    "\\Common7\\Tools\\VsDevCmd.bat"
__DEFAULT_SETTINGS["WINDOWS_MSVC_CMAKE_PATH" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] =               \
    __DEFAULT_SETTINGS["WINDOWS_MSVC_BUILD_TOOLS_PATH" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] +     \
    "\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe"
__DEFAULT_SETTINGS["WINDOWS_MSVC_CMAKE_GENERATOR" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] =          \
    "Visual Studio 17 2022"
__DEFAULT_SETTINGS["WINDOWS_MSVC_SETUP_GUARD_TIME_SECONDS"] = 60 * 10
__DEFAULT_SETTINGS["WINDOWS_CMAKE_GUARD_TIME_SECONDS"] = 60 * 10
__DEFAULT_SETTINGS["WINDOWS_BUILD_SUBDIR"] = "build"
__DEFAULT_SETTINGS["WINDOWS_BUILD_GUARD_TIME_SECONDS"] = 60 * 30
# u_run.py
__DEFAULT_SETTINGS["BRANCH_DEFAULT"] = "origin/master"
# u_utils.py
__DEFAULT_SETTINGS["INSTALL_LOCK_WAIT_SECONDS"] = 60*60
__DEFAULT_SETTINGS["UNITY_URL"] = "https://github.com/ThrowTheSwitch/Unity"
__DEFAULT_SETTINGS["UNITY_SUBDIR"] = "Unity"
__DEFAULT_SETTINGS["JLINK_SWO_PORT"] = 19021
__DEFAULT_SETTINGS["TIME_FORMAT"] = "%Y-%m-%d_%H:%M:%S"
__DEFAULT_SETTINGS["DOWNLOAD_GUARD_TIME_SECONDS"] = 120
__DEFAULT_SETTINGS["RUN_GUARD_TIME_SECONDS"] = 60 * 60
__DEFAULT_SETTINGS["RUN_INACTIVITY_TIME_SECONDS"] = 60 * 5
__DEFAULT_SETTINGS["FILTER_MACRO_NAME"] = "U_CFG_APP_FILTER"
__DEFAULT_SETTINGS["EXE_RUN_QUEUE_WAIT_SECONDS"] = 1

# Switches that can be controlled under automation
__DEFAULT_SETTINGS["SWITCH_LIST" + __SETTINGS_POSTFIX_AGENT_SPECIFIC] =    \
    [{"name":"RF_SWITCH_A", "ip":"10.10.2.212", "0":":SETA=0", "1":":SETA=1"},
     {"name":"PWR_SWITCH_A", "ip":"10.10.2.210", "0":"FF0700", "1":"FF0701"}]

# Other stuff
__USER_INTERVENTION_REQUIRED = False
__FORCE_WRITE = False
__HOLD_THE_BUS = False
__READ_SETTINGS = {}
__FILTERED_READ_SETTINGS = {}
__UPDATED_READ_SETTINGS = {}
__WRITE_SETTINGS = {}
__SETTINGS = {}

def __replace_env_var(entry):
    '''Use this function to expand environmental variables in strings.
       The entry can be any object and for dicts and lists this function
       will recursivly iterate through all their entries to find every
       string.'''
    if isinstance(entry, str):
        entry = os.path.expandvars(entry)
    elif isinstance(entry, dict):
        # This is a dictonary - iterate through it and recursivly call
        # __replace_env_var() replace any environmental variable in it
        for __key in entry:
            entry[__key] = __replace_env_var(entry[__key])
    elif isinstance(entry, list):
        # This is a list - iterate through it and recursivly call
        # __replace_env_var() replace any environmental variable in it
        entry[:] = [__replace_env_var(__value) for __value in entry]
    return entry

# Make sure the settings directory exists before we try to create a file lock
if not os.path.exists(__SETTINGS_FILE_DIRECTORY):
    os.makedirs(__SETTINGS_FILE_DIRECTORY)

# Ensure exclusive access to avoid collisions when this is included
# in many scripts, potentially running in their own processes.
with portalocker.Lock(__SETTINGS_FILE_DIRECTORY + os.sep + "settings.lock",
                      "w", portalocker.LOCK_EX):
    # Read settings from the global configuration files
    if os.path.isfile(__SETTINGS_FILE_PATH_GENERAL):
        try:
            with open(__SETTINGS_FILE_PATH_GENERAL, encoding="utf8") as f:
                __READ_SETTINGS = json.load(f)
        except Exception as ex:
            print("u_settings: ************************** WARNING ***************************")
            print("u_settings: error {}{} reading settings file \"{}\".".    \
                  format(type(ex).__name__, str(ex), __SETTINGS_FILE_PATH_GENERAL))
            print("u_settings: ************************** WARNING ***************************")
            # Most likely the user has just inserted a syntax
            # error and they won't want us writing the defaults
            # over all their good stuff
            __HOLD_THE_BUS = True
    if os.path.isfile(__SETTINGS_FILE_PATH_AGENT_SPECIFIC):
        try:
            with open(__SETTINGS_FILE_PATH_AGENT_SPECIFIC, encoding="utf8") as f:
                __READ_SETTINGS.update(json.load(f))
        except Exception as ex:
            print("u_settings: ************************** WARNING ***************************")
            print("u_settings: error {}{} reading settings file \"{}\".".    \
                  format(type(ex).__name__, str(ex), __SETTINGS_FILE_PATH_AGENT_SPECIFIC))
            print("u_settings: ************************** WARNING ***************************")
            # Most likely the user has just inserted a syntax
            # error and they won't want us writing the defaults
            # over all their good stuff
            __HOLD_THE_BUS = True
    if not __READ_SETTINGS:
        # For backwards-compatibility
        if os.path.isfile(__SETTINGS_FILE_PATH_OLD):
            try:
                with open(__SETTINGS_FILE_PATH_OLD, encoding="utf8") as f:
                    __READ_SETTINGS.update(json.load(f))
                    __FORCE_WRITE = True
            except Exception as ex:
                print("u_settings: ************************** WARNING ***********************")
                print("u_settings: error {}{} reading old settings file \"{}\".".    \
                      format(type(ex).__name__, str(ex), __SETTINGS_FILE_PATH_OLD))
                print("u_settings: ************************** WARNING ***********************")

    # Having set up the default values and [potentially] read
    # settings from file, decide what to do:
    #
    # a) if __READ_SETTINGS is empty, use the defaults and
    #    write them back to file again (though see also (1)
    #    to (4) below concerning what is written-back),
    # b) if we have __READ_SETTINGS but the value of
    #    "SETTINGS_VERSION" from there is different to the
    #    one in __DEFAULT_SETTINGS then use the defaults and
    #    do not write those defaults back to disk: the version
    #    change indicates that user intervention is required;
    #    one of the Python scripts that includes this script
    #    (e.g. u_agent.py) will check for this and not allow
    #    the agent to run until the planets are aligned once
    #    more,
    # c) else use the values from __READ_SETTINGS (but see
    #    also (3) and (4) below).
    #
    # In addition to the above:
    #
    # 1) if we find an entry in __DEFAULT_SETTINGS which ends
    #    in "_TEST_ONLY_TEMP", e.g. "THINGY_TEST_ONLY_TEMP",
    #    then merge the bit before, e.g. "THINGY", into the
    #    settings we adopt and do NOT write that particular
    #    value back to disk: the user is trying something out,
    #    potentially replacing an existing setting, and doesn't
    #    want to infect the global configuration file with it,
    # 2) if we find an entry in __DEFAULT_SETTINGS which ends
    #    in "_AGENT_SPECIFIC", e.g. "PATH_X_AGENT_SPECIFIC",
    #    and we do not have an entry in __READ_SETTINGS that
    #    begins with the bit before the prefix, e.g. "PATH_X"
    #    then merge the bit before into the settings we adopt
    #    and merge "PATH_X_FIX_ME" into the value we write back
    #    to disk: again, someone must update the global
    #    configuration file on the agent with a value for "PATH_X"
    #    and a script such as u_agent.py which includes this
    #    script will not allow the agent to run until that is done,
    # 3) if we find an entry in __READ_SETTINGS which ends in
    #    "_AGENT_SPECIFIC", "_FIX_ME" or "_TEST_ONLY_TEMP" then
    #    remove it from the settings we use,
    # 4) if we find an entry in __DEFAULT_SETTINGS which is
    #    is not present in __READ_SETTINGS after doing the (1)
    #    and (2) modifications on it then merge it into
    #    the values we use (in its modified form).

    # First sort out the settings we would use based on the defaults
    for __key in __DEFAULT_SETTINGS:
        # Condition (4) above is assumed
        __SETTINGS_KEY = __key
        __REWRITE_KEY = __key
        __VALUE = __DEFAULT_SETTINGS[__key]
        if __SETTINGS_KEY.endswith(__SETTINGS_POSTFIX_TEST_ONLY_TEMP):
            # Condition (1) above
            __SETTINGS_KEY = __SETTINGS_KEY.replace(__SETTINGS_POSTFIX_TEST_ONLY_TEMP, "")
            __REWRITE_KEY = None
        if __SETTINGS_KEY.endswith(__SETTINGS_POSTFIX_AGENT_SPECIFIC):
            # Condition (2) above
            __SETTINGS_KEY = __SETTINGS_KEY.replace(__SETTINGS_POSTFIX_AGENT_SPECIFIC, "")
            if __SETTINGS_KEY not in __READ_SETTINGS:
                __USER_INTERVENTION_REQUIRED = True
        # Populate __SETTINGS and __WRITE_SETTINGS
        __SETTINGS[__SETTINGS_KEY] = __VALUE
        if __REWRITE_KEY:
            __WRITE_SETTINGS[__REWRITE_KEY] = __VALUE

    # We now have a __SETTINGS list based on __DEFAULT_SETTINGS
    # and __WRITE_SETTINGS contains what we could write back
    # Next sort out __READ_SETTINGS
    if __READ_SETTINGS:
        # Conditions (b) and (c) above
        if "SETTINGS_VERSION" not in __READ_SETTINGS or  \
           __READ_SETTINGS["SETTINGS_VERSION"] == __DEFAULT_SETTINGS["SETTINGS_VERSION"]:
            # Condition (c) above: we're going to use __READ_SETTINGS
            # so sort out conditions (3) and (4) above
            # First, condition (3)
            __FILTERED_READ_SETTINGS = {}
            for __key in __READ_SETTINGS:
                if not __key.endswith(__SETTINGS_POSTFIX_TEST_ONLY_TEMP) and \
                   not __key.endswith(__SETTINGS_POSTFIX_FIX_ME) and         \
                   not __key.endswith(__SETTINGS_POSTFIX_AGENT_SPECIFIC):
                    __FILTERED_READ_SETTINGS[__key] = __READ_SETTINGS[__key]
            # Then condition (4)
            __UPDATED_READ_SETTINGS = __FILTERED_READ_SETTINGS.copy()
            for __key in __SETTINGS:
                if __key not in __UPDATED_READ_SETTINGS:
                    __UPDATED_READ_SETTINGS[__key] = __SETTINGS[__key]
            # And finally, condition (1) also needs handling here,
            # so that the user can override values read from the
            # settings file while testing
            for __key in __DEFAULT_SETTINGS:
                __key_root = __key.replace(__SETTINGS_POSTFIX_TEST_ONLY_TEMP, "")
                if __key.endswith(__SETTINGS_POSTFIX_TEST_ONLY_TEMP) and \
                    __key_root in __UPDATED_READ_SETTINGS:
                    __UPDATED_READ_SETTINGS[__key_root] = __DEFAULT_SETTINGS[__key]
        else:
            # Condition (b) above
            __USER_INTERVENTION_REQUIRED = True
            __WRITE_SETTINGS = {}
    else:
        # Condition (a) above
        print(f"u_settings: no settings at \"{__SETTINGS_FILE_PATH_GENERAL}\","              \
              f"\"{__SETTINGS_FILE_PATH_AGENT_SPECIFIC}\" or \"{__SETTINGS_FILE_PATH_OLD}\"" \
              " using defaults.")

    # If we have __UPDATED_READ_SETTINGS we use it instead of __SETTINGS
    if __UPDATED_READ_SETTINGS:
        __SETTINGS = __UPDATED_READ_SETTINGS

    # Finally need to sort out the [re]writing.
    if __WRITE_SETTINGS and not __HOLD_THE_BUS:
        # Where something is already present in __READ_SETTINGS
        # then use the value from __READ_SETTINGS
        __LISTS_DIFFER = __FORCE_WRITE
        __local_write_settings = {}
        for __key in __WRITE_SETTINGS:
            __key_root = __key.replace(__SETTINGS_POSTFIX_AGENT_SPECIFIC, "")
            if __key_root in __READ_SETTINGS:
                __local_write_settings[__key] = __READ_SETTINGS[__key_root]
            else:
                __LISTS_DIFFER = True
                __local_write_settings[__key] = __WRITE_SETTINGS[__key]
                if __key_root != __key:
                    print(f"u_settings: *** WARNING agent specific setting {__key_root}"    \
                          " not found in settings file.")
        __WRITE_SETTINGS = __local_write_settings

        # Don't want to lose the user's stuff, so add anything that
        # we don't understand to the write list
        for __key in __FILTERED_READ_SETTINGS:
            if __key not in __WRITE_SETTINGS and                                   \
               (__key + __SETTINGS_POSTFIX_AGENT_SPECIFIC) not in __WRITE_SETTINGS:
                __WRITE_SETTINGS[__key] = __FILTERED_READ_SETTINGS[__key]

        if not __LISTS_DIFFER:
            # Check if any of the values we've ended up with are different
            for __key in __WRITE_SETTINGS:
                if __WRITE_SETTINGS[__key] != __READ_SETTINGS[__key.replace(__SETTINGS_POSTFIX_AGENT_SPECIFIC, "")]:
                    __LISTS_DIFFER = True
                    break
        if __LISTS_DIFFER:
            # Either there's a new entry or a value is different,
            # write the now merged list into the global settings files
            __write_settings_general = {}
            __write_settings_agent_specific = {}
            for __key in __WRITE_SETTINGS:
                if __key.endswith(__SETTINGS_POSTFIX_AGENT_SPECIFIC):
                    __key_root = __key.replace(__SETTINGS_POSTFIX_AGENT_SPECIFIC,"")
                    if __FILTERED_READ_SETTINGS and __key_root in __FILTERED_READ_SETTINGS:
                        __write_settings_agent_specific[__key_root] = __WRITE_SETTINGS[__key]
                    else:
                        # Writing the __key with the post-fix "__FIX_ME"
                        __write_settings_agent_specific[__key_root + __SETTINGS_POSTFIX_FIX_ME] = __WRITE_SETTINGS[__key]
                else:
                    __write_settings_general[__key] = __WRITE_SETTINGS[__key]
            if not os.path.isdir(os.path.expanduser(__SETTINGS_FILE_DIRECTORY)):
                os.makedirs(os.path.expanduser(__SETTINGS_FILE_DIRECTORY))
            if __write_settings_general:
                print("u_settings: creating/re-writing global settings file \"{}\".". \
                      format(__SETTINGS_FILE_PATH_GENERAL))
                with open(__SETTINGS_FILE_PATH_GENERAL, 'w', encoding='utf8') as out:
                    json.dump(__write_settings_general, out, indent=2)
            if __write_settings_agent_specific:
                print("u_settings: creating/re-writing global settings file \"{}\".". \
                      format(__SETTINGS_FILE_PATH_AGENT_SPECIFIC))
                with open(__SETTINGS_FILE_PATH_AGENT_SPECIFIC, 'w', encoding='utf8') as out:
                    json.dump(__write_settings_agent_specific, out, indent=2)

    # Populate this module with settings
    __current_module = sys.modules[__name__]
    for __key in __SETTINGS:
        __value = __replace_env_var(__SETTINGS[__key])
        #print(f"u_settings: \"{__key}\" = \"{__value}\"")
        setattr(__current_module, __key, __value)

def user_intervention_required():
    '''Return true if a human needs to sort out the global settings files'''
    return __USER_INTERVENTION_REQUIRED
