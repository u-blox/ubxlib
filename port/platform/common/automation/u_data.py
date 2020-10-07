#!/usr/bin/env python

'''Get the ubxlib instance data from a table in a .md file.'''

# The file that contains the instance data must be
# a .md file with a table in it containing the data.
# The instance data must be the only table in the file
# and must be in Markdown format as follows:
#
# | instance | description | platform | sdk | module(s) | APIs supported | #defines |
#
# ... where:
#
# instance          is a set of integers x or x.y or x.y.z,
# description       is a textual description,
# platform          is the name of the platform, e.g. ESP32 or NRF52 or STM32F4,
# sdk               is the name of an SDK for that platform, e.g. SES or GCC for NRF52,
# module(s)         are the modules connected to the MCU on that platform,
#                   e.g. SARA-R412M-03B, NINA-B3, ZOE-M8; where there is more
#                   than one spaces should be used as separators,
# APIs supported    are the APIs that should run on that platform separated by
#                   spaces, e.g. port net mqtt
# #defines          are the #defines to be applied for this instance, separated by
#                   spaces, e.g. MY_FLAG U_CFG_APP_PIN_CELLULAR_ENABLE_POWER=-1
#
# The outer column separators may be omitted.

# Prefix to put at the start of all prints
PROMPT = "u_data: "

# The file that contains the instance data as a table
# in Markdown format
DATA_FILE = "DATABASE.md"

# The prefix to add to a cellular module to
CELLULAR_MODULE_TYPE_PREFIX = "U_CELL_MODULE_TYPE_"

def get(filename):
    '''Read the instance database from a table in a .md file'''
    database = []
    row = {}
    instance = []

    print("{}getting instance data from file \"{}\"...".format(PROMPT, filename))
    file_handle = open(filename, "r")
    # Read lines from the file until we hit a row of our table,
    # which is defined as a line with at least six '|'
    # characters in it
    contents = file_handle.read()
    lines = contents.splitlines()
    for line in lines:
        items = line.split("|")
        if len(items) >= 6:
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
                        # Platform
                        row["platform"] = stripped
                        index += 1
                    elif index == 3:
                        # SDK
                        row["sdk"] = stripped
                        index += 1
                    elif index == 4:
                        # Modules
                        row["modules"] = stripped.split()
                        index += 1
                    elif index == 5:
                        # APIs
                        row["apis"] = stripped.split()
                        index += 1
                    elif index == 6:
                        # #defines
                        row["defines"] = stripped.split()
                        index += 1
                        database.append(row.copy())

    file_handle.close()
    return database

def display(database):
    '''Print out the instances from database'''

    print("{} {} instance(s) found:".format(PROMPT, len(database)))
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
        item += ": \"{}\"".format(row["description"])
        # Then platform
        if row["platform"] != "":
            item += " {} platform with".format(row["platform"])
        else:
            item += " with"
        # Then SDK
        if row["sdk"] != "":
            item += " SDK \"{}\"".format(row["sdk"])
        else:
            item += " no SDK,"
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
        print("{}.".format(item))

def get_instances_for_platform(database, platform):
    '''Return a list of instances that support the given platform'''
    instances = []

    for row in database:
        if row["platform"].lower() == platform.lower():
            instances.append(row["instance"][:])

    return instances

def get_instances_for_platform_sdk(database, platform, sdk):
    '''Return a list of instances that support a platform/SDK combination'''
    instances = []

    for row in database:
        if (row["platform"].lower() == platform.lower()) and \
           (row["sdk"].lower() == sdk.lower()):
            instances.append(row["instance"][:])

    return instances

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

    return platform

def get_cellular_module_for_instance(database, instance):
    '''Return the cellular module that is used in the given instance'''
    module_name = None

    for row in database:
        if instance == row["instance"]:
            if row["modules"]:
                for module in row["modules"]:
                    # SARA is assumed to be a cellular module
                    if module.startswith("SARA"):
                        module_name = CELLULAR_MODULE_TYPE_PREFIX + module
                        break;

    return module_name

def get_defines_for_instance(database, instance):
    '''Return the defines that are required by the given instance'''
    defines = None

    for row in database:
        if instance == row["instance"]:
            defines = row["defines"]

    return defines

def get_sdk_for_instance(database, instance):
    '''Return the sdk for the given instance'''
    sdk = None

    for row in database:
        if instance == row["instance"]:
            sdk = row["sdk"]

    return sdk

def get_description_for_instance(database, instance):
    '''Return the description for the given instance'''
    description = None

    for row in database:
        if instance == row["instance"]:
            description = row["description"]

    return description

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
