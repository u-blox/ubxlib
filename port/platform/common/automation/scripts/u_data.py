#!/usr/bin/env python

'''Get the ubxlib instance data from a table in a .md file.'''

from scripts import u_settings

# The file that contains the instance data must be
# a .md file with a table in it containing the data.
# The instance data must be the only table in the file
# and must be in Markdown format as follows:
#
# | instance | description | duration | MCU | board | platform | toolchain | module(s) | APIs supported | #defines |
#
# ... where:
#
# instance          is a set of integers x or x.y or x.y.z,
# description       is a textual description,
# duration          the estimated run-time of the instance in minutes,
# MCU               is the name of the MCU, e.g. ESP32 or NRF52 or STM32F4,
# board             the name of the board, only used by the Zephyr platform,
# platform          is the name of the platform, e.g. ESP-IDF or STM32Cube,
# toolchain         is the name of a toolchain for that platform, e.g. SES or GCC for nRF5,
# module(s)         are the modules connected to the MCU on that platform,
#                   e.g. SARA-R412M-03B, NINA-B3, M8; where there is more
#                   than one spaces should be used as separators,
# APIs supported    are the APIs that should run on that platform separated by
#                   spaces, e.g. port net mqtt
# #defines          are the #defines to be applied for this instance, separated by
#                   spaces, e.g. MY_FLAG U_CFG_APP_PIN_CELLULAR_ENABLE_POWER=-1
#
# The outer column separators may be omitted, case is ignored.

# Prefix to put at the start of all prints
PROMPT = "u_data: "

# The prefix(es) that identify a cellular module
CELLULAR_MODULE_STARTS_WITH = ["SARA", "LARA"]

# The prefix(es) that identify a short-range module
SHORT_RANGE_MODULE_STARTS_WITH = ["NINA", "ANNA", "ODIN"]

# The file that contains the instance data as a table
# in Markdown format
DATA_FILE = u_settings.DATA_FILE #DATABASE.md

# The prefix to add to a cellular module
CELLULAR_MODULE_TYPE_PREFIX = u_settings.CELLULAR_MODULE_TYPE_PREFIX

# The prefix to add to a short range module
SHORT_RANGE_MODULE_TYPE_PREFIX = u_settings.SHORT_RANGE_MODULE_TYPE_PREFIX

# The prefix to add to a GNSS module
GNSS_MODULE_TYPE_PREFIX = u_settings.GNSS_MODULE_TYPE_PREFIX

def get(filename):
    '''Read the instance database from a table in a .md file'''
    database = []
    row = {}
    instance = []

    file_handle = open(filename, "r", encoding="utf8")
    # Read lines from the file until we hit a row of our table,
    # which is defined as a line with at least six '|'
    # characters in it
    contents = file_handle.read()
    lines = contents.splitlines()
    for line in lines:
        items = line.split("|")
        if len(items) >= 7:
            index = 0
            row.clear()
            for item in items:
                # Find the instance item,
                # should begin with a numeral
                stripped = item.strip()
                if stripped[:1].isdigit() and index == 0:
                    # Parse out the numbers and
                    # add them to the row database
                    # as a dictionary item
                    numbers = stripped.split(".")
                    del instance[:]
                    for number in numbers:
                        instance.append(int(number))
                    row["instance"] = instance[:]
                    index += 1
                else:
                    # Deal with the other items
                    if index == 1:
                        # Description
                        row["description"] = stripped
                        index += 1
                    elif index == 2:
                        # Duration
                        row["duration"] = stripped
                        index += 1
                    elif index == 3:
                        # MCU
                        row["mcu"] = stripped
                        index += 1
                    elif index == 4:
                        # Board
                        row["board"] = stripped
                        index += 1
                    elif index == 5:
                        # Platform
                        row["platform"] = stripped
                        index += 1
                    elif index == 6:
                        # Toolchain
                        row["toolchain"] = stripped
                        index += 1
                    elif index == 7:
                        # Modules
                        row["modules"] = stripped.split()
                        index += 1
                    elif index == 8:
                        # APIs
                        row["apis"] = stripped.split()
                        index += 1
                    elif index == 9:
                        # #defines
                        row["defines"] = stripped.split()
                        index += 1
                        database.append(row.copy())

    file_handle.close()
    return database

def display(database):
    '''Print out the instances from database'''

    print(f"{PROMPT} {len(database)} instance(s) found:")
    for row in database:
        # Instance first
        item = ""
        for idx, number in enumerate(row["instance"]):
            if idx == 0:
                item += str(number)
            else:
                item += "." + str(number)
        item = item.rjust(8)
        # Then description
        item += f": \"{row['description']}\""
        # Then duration
        if row["duration"] != "":
            item += f" {row['duration']} duration with"
        else:
            item += " with"
        # Then MCU
        if row["mcu"] != "":
            item += f" {row['mcu']} MCU with"
        else:
            item += " with"
        # Then board
        if row["board"] != "":
            item += f" {row['board']} board with"
        else:
            item += " with"
        # Then platform
        if row["platform"] != "":
            item += f" {row['platform']} platform with"
        else:
            item += " with"
        # Then toolchain
        if row["toolchain"] != "":
            item += f" toolchain \"{row['toolchain']}\""
        else:
            item += " default toolchain,"
        # Then modules
        if row["modules"]:
            item += " and"
            for idx, module in enumerate(row["modules"]):
                if idx == 0:
                    item += " " + module
                else:
                    item += ", " + module
        else:
            item += " no"
        item += " module(s) supporting"
        # Then APIs
        if row["apis"]:
            item += " the API(s)"
            for idx, api in enumerate(row["apis"]):
                if idx == 0:
                    item += " \"" + api + "\""
                else:
                    item += ", \"" + api + "\""
        else:
            item += " no APIs"
        # Then the #defines
        if row["defines"]:
            item += " with required #define(s)"
            for idx, define in enumerate(row["defines"]):
                if idx == 0:
                    item += " " + define
                else:
                    item += ", " + define
        else:
            item += " with no required #defines"
        print(f"{item}.")

def get_instances_for_mcu(database, mcu):
    '''Return a list of instances that support the given MCU'''
    instances = []

    for row in database:
        if row["mcu"].lower() == mcu.lower():
            instances.append(row["instance"][:])

    return instances

def get_instances_for_platform_mcu_toolchain(database, platform, mcu, toolchain):
    '''Return a list of instances that support a platform/MCU/toolchain combination'''
    instances = []

    for row in database:
        if (row["platform"].lower() == platform.lower()) and \
           (mcu is None or (row["mcu"].lower() == mcu.lower())) and \
           (toolchain is None or (row["toolchain"].lower() == toolchain.lower())):
            instances.append(row["instance"][:])

    return instances

def get_toolchains_for_platform_mcu(database, platform, mcu):
    '''Return the toolchains for the given platform and MCU combination'''
    toolchains = []

    for row in database:
        if (row["platform"].lower() == platform.lower()) and \
           (mcu is None or (row["mcu"].lower() == mcu.lower())) and \
           row["toolchain"] is not None:
            toolchains.append(row["toolchain"])

    return toolchains

def get_instances_for_api(database, api):
    '''Return a list of instances that support the given API'''
    instances = []

    for row in database:
        found = False
        for _api in row["apis"]:
            if not found and (_api.lower() == api.lower()):
                instances.append(row["instance"][:])
                found = True

    return instances

def get_instances_all(database):
    '''Return all instances'''
    instances = []

    for row in database:
        instances.append(row["instance"][:])

    return instances

def get_platform_for_instance(database, instance):
    '''Return the platform that is used by the given instance'''
    platform = None

    for row in database:
        if instance == row["instance"]:
            platform = row["platform"]
            break

    return platform

def get_cellular_module_for_instance(database, instance):
    '''Return the cellular module that is used in the given instance'''
    module_name = None

    for row in database:
        if instance == row["instance"]:
            if row["modules"]:
                for module in row["modules"]:
                    for starts_with in CELLULAR_MODULE_STARTS_WITH:
                        if module.startswith(starts_with):
                            module_name = CELLULAR_MODULE_TYPE_PREFIX + module
                            break
                    if module_name:
                        break
    return module_name

def get_short_range_module_for_instance(database, instance):
    '''Return the short-range module that is used in the given instance'''
    module_name = None

    for row in database:
        if instance == row["instance"]:
            if row["modules"]:
                for module in row["modules"]:
                    for starts_with in SHORT_RANGE_MODULE_STARTS_WITH:
                        if module.startswith(starts_with):
                            module_name = SHORT_RANGE_MODULE_TYPE_PREFIX + module
                            break
                    if module_name:
                        break
    return module_name

def get_gnss_module_for_instance(database, instance):
    '''Return the GNSS module that is used in the given instance'''
    module_name = None

    for row in database:
        if instance == row["instance"]:
            if row["modules"]:
                for module in row["modules"]:
                    could_be_gnss = True
                    for starts_with in CELLULAR_MODULE_STARTS_WITH:
                        if module.startswith(starts_with):
                            could_be_gnss = False
                            break
                    if could_be_gnss:
                        for starts_with in SHORT_RANGE_MODULE_STARTS_WITH:
                            if module.startswith(starts_with):
                                could_be_gnss = False
                                break
                    if could_be_gnss:
                        module_name = GNSS_MODULE_TYPE_PREFIX + module
                        break
    return module_name

def get_defines_for_instance(database, instance):
    '''Return the defines that are required by the given instance'''
    defines = None
    bandmask_already_defined = False

    for row in database:
        if instance == row["instance"]:
            defines = row["defines"]

    if not defines:
        defines = []

    # If there is a cellular module on this instance, add its
    # name to the defines list
    cellular_module_name = get_cellular_module_for_instance(database, instance)
    if cellular_module_name:
        defines.append("U_CFG_TEST_CELL_MODULE_TYPE=" + cellular_module_name)

    # If there is a short-range module on this instance, add its
    # name to the defines list
    short_range_module_name = get_short_range_module_for_instance(database, instance)
    if short_range_module_name:
        defines.append("U_CFG_TEST_SHORT_RANGE_MODULE_TYPE=" + short_range_module_name)

    # If there is a GNSS module on this instance, add its
    # name to the defines list
    gnss_module_name = get_gnss_module_for_instance(database, instance)
    if gnss_module_name:
        defines.append("U_CFG_TEST_GNSS_MODULE_TYPE=" + gnss_module_name)

    # Also, when running testing it is best to run the
    # the "port" tests first as, if there's a problem with the
    # port, you want to notice it first.
    # This also acts as a flag to indicate that we're running
    # under u_runner automation
    defines.append("U_RUNNER_TOP_STR=port")

    for define in defines:
        if define.startswith("U_CELL_TEST_CFG_BANDMASK1"):
            bandmask_already_defined = True
            break
    if not bandmask_already_defined:
        # When running tests on cellular LTE modules, so
        # SARA-R4 or SARA-R5, we need to set the RF band we
        # are running in to NOT include the public network,
        # since otherwise the modules can sometimes wander off
        # onto it.
        defines.append("U_CELL_TEST_CFG_BANDMASK1=0x000010ULL")

    return defines

def get_toolchain_for_instance(database, instance):
    '''Return the toolchain for the given instance'''
    toolchain = None

    for row in database:
        if instance == row["instance"]:
            toolchain = row["toolchain"]
            break

    return toolchain

def get_mcu_for_instance(database, instance):
    '''Return the MCU for the given instance'''
    mcu = None

    for row in database:
        if instance == row["instance"]:
            mcu = row["mcu"]
            break

    return mcu

def get_board_for_instance(database, instance):
    '''Return the board for the given instance'''
    board = None

    for row in database:
        if instance == row["instance"]:
            board = row["board"]
            break

    return board

def get_description_for_instance(database, instance):
    '''Return the description for the given instance'''
    description = None

    for row in database:
        if instance == row["instance"]:
            description = row["description"]

    return description

def get_duration_for_instance(database, instance):
    '''Return the expected run duration for the given instance'''
    duration = None

    for row in database:
        if instance == row["instance"]:
            duration = int(row["duration"])

    return duration

def api_in_database(database, api):
    '''Return true if the given api is in the database'''
    is_in_database = False

    for row in database:
        if not is_in_database:
            for _api in row["apis"]:
                if _api == api:
                    is_in_database = True
                    break

    return is_in_database
