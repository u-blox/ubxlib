# Hack for importing the automations scripts...
from scripts import u_utils
from sys import path as sys_path
from os import environ
sys_path.append(f"{u_utils.AUTOMATION_DIR}")

import u_data, u_connection, u_select # From automation
from u_utils import MONITOR_DTR_RTS_OFF_MARKER, commit_message_parse, merge_filter, ENV_UBXLIB_AUTO

from invoke import task, Exit
from pathlib import PurePath
from scripts.packages import u_package
from scripts import u_utils
from . import nrf5, esp_idf, nrfconnect, stm32cubef4, arduino
from enum import Enum
import sys
import json

DATABASE = PurePath(u_utils.AUTOMATION_DIR, "DATABASE.md").as_posix()

# The environment variable that may contain some defines
# we should use
UBXLIB_DEFINES_VAR = "U_UBXLIB_DEFINES"

class Command(Enum):
    BUILD = 1
    FLASH = 2
    LOG = 3

def eprint(*args, **kwargs):
    """Helper function for writing to stderr"""
    print(*args, file=sys.stderr, **kwargs)

def parse_instance(str):
    try:
        # Convert a string like "13.0.0" to a Python int list: [13,0,0]
        instance = list(map(int, str.split('.')))
    except:
        raise Exit(f"Invalid instance format: '{str}'")
    return instance

def instance_command(ctx, instance_str, cmd):
    db_data = u_data.get(DATABASE)
    instance = parse_instance(instance_str)

    # It is sometimes useful for the platform tools to be able
    # to detect that they are running under automation (e.g. this
    # is used to switch ESP-IDF to using u_runner rather than the
    # usual ESP-IDF unit test menu system).
    # For this purpose we add ENV_UBXLIB_AUTO to the environment
    environ[ENV_UBXLIB_AUTO] = "1"

    # Read out instance info from DATABASE.md
    platform = u_data.get_platform_for_instance(db_data, instance)
    if not platform:
        raise Exit(f"Unknown platform for: '{instance_str}'")
    board = u_data.get_board_for_instance(db_data, instance)
    defines = u_data.get_defines_for_instance(db_data, instance)
    mcu = u_data.get_mcu_for_instance(db_data, instance)

    # Defines may be provided via an environment
    # variable, in a list separated with semicolons, e.g.:
    # set U_UBXLIB_DEFINES=THING_1;ANOTHER_THING=123;ONE_MORE=boo
    # Add these in.
    if UBXLIB_DEFINES_VAR in environ and environ[UBXLIB_DEFINES_VAR].strip():
        defines.extend(environ[UBXLIB_DEFINES_VAR].strip().split(";"))

    # Merge in any filter string we might have
    if cmd == Command.BUILD and ctx.filter:
        defines = merge_filter(defines, ctx.filter)

    # For ESP targets: Check if RTS and DTR should be set when opening log UART
    monitor_dtr_rts_on = None
    if MONITOR_DTR_RTS_OFF_MARKER in defines:
        monitor_dtr_rts_on = False

    # Get debugger serial
    connection = u_connection.get_connection(instance)
    serial = ""
    if connection and "debugger" in connection and connection["debugger"]:
        serial = connection["debugger"]

    platform = platform.lower()
    if platform == "nrf5sdk":
        nrf5.check_installation(ctx)
        if cmd == Command.BUILD:
            nrf5.build(ctx, output_name="", build_dir=ctx.build_dir, u_flags=defines)
        elif cmd == Command.FLASH:
            nrf5.flash(ctx, output_name="", build_dir=ctx.build_dir, debugger_serial=serial)
        elif cmd == Command.LOG:
            nrf5.log(ctx, debugger_serial=serial)

    elif platform == "zephyr":
        nrfconnect.check_installation(ctx)
        if cmd == Command.BUILD:
            nrfconnect.build(ctx, board_name=board, output_name="", build_dir=ctx.build_dir, u_flags=defines)
        elif cmd == Command.FLASH:
            hex_file = None
            if mcu.lower() == "nrf5340":
                # The hex file to flash is specified in zephyr/runners.yml for zephyr and usually set to "zephyr.hex".
                # For MCUs with multiple cores such as nRF5340 merged_domains.hex is used instead which includes
                # multiple firmwares. The problem in this case that when merged_domains.hex is used it is specified
                # as a full absoulte path. This becomes a problem in the Jenkins case where the firmware is built
                # on one machine and flashed on another. For this reason we will pass the hex path manually here.
                hex_file = f"{ctx.build_dir}/zephyr/merged_domains.hex"
            nrfconnect.flash(ctx, output_name="", build_dir=ctx.build_dir, debugger_serial=serial, hex_file=hex_file)
        elif cmd == Command.LOG:
            nrfconnect.log(ctx, debugger_serial=serial)

    elif platform == "esp-idf":
        esp_idf.check_installation(ctx)
        if cmd == Command.BUILD:
            esp_idf.build(ctx, output_name="", build_dir=ctx.build_dir, u_flags=defines)
        elif cmd == Command.FLASH:
            esp_idf.flash(ctx, serial_port=connection["serial_port"],
                          output_name="", build_dir=ctx.build_dir)
        elif cmd == Command.LOG:
            esp_idf.log(ctx, serial_port=connection["serial_port"],
                        dtr_state=monitor_dtr_rts_on,
                        rts_state=monitor_dtr_rts_on)

    elif platform == "stm32cube":
        stm32cubef4.check_installation(ctx)
        if cmd == Command.BUILD:
            stm32cubef4.build(ctx, output_name="", build_dir=ctx.build_dir, u_flags=defines)
        elif cmd == Command.FLASH:
            stm32cubef4.flash(ctx, output_name="", build_dir=ctx.build_dir, debugger_serial=serial)
        elif cmd == Command.LOG:
            port = instance[0] + 40404
            stm32cubef4.log(ctx, debugger_serial=serial, port=port)

    elif platform == "arduino":
        arduino.check_installation(ctx)
        if cmd == Command.BUILD:
            arduino.build(ctx, libraries_dir=f"{ctx.build_dir}/libraries", board=board,
                          output_name="app", build_dir=ctx.build_dir, u_flags=defines)
        elif cmd == Command.FLASH:
            arduino.flash(ctx, serial_port=connection["serial_port"], board=board,
                          output_name="app", build_dir=ctx.build_dir)
        elif cmd == Command.LOG:
            port = instance[0] + 40404
            arduino.log(ctx, serial_port=connection["serial_port"],
                        dtr_state=monitor_dtr_rts_on,
                        rts_state=monitor_dtr_rts_on)

    else:
        raise Exit(f"Unsupported platform: '{platform}'")


@task()
def export(ctx):
    """Output the u_package environment"""
    pkg_cfg = u_package.get_u_packages_config(ctx)
    for pkg_name in pkg_cfg:
        print(f'export U_PKG_{pkg_name.upper()}={pkg_cfg[pkg_name]["package_dir"]}')


@task()
def install_all(ctx):
    """Makes sure all packages are installed"""
    pkg_names = []
    pkg_cfg = u_package.get_u_packages_config(ctx)
    for pkg_name in pkg_cfg:
        pkg_names.append(pkg_name)
    u_package.load(ctx, pkg_names)

@task()
def build(ctx, instance, build_dir=None, filter=None):
    """Build the firmware for an automation instance"""
    if not build_dir:
        build_dir = f'_build/automation/{instance}'
    ctx.build_dir = build_dir
    ctx.filter = filter
    instance_command(ctx, instance, Command.BUILD)

@task()
def flash(ctx, instance, build_dir=None):
    """Flash the firmware for an automation instance"""
    if not build_dir:
        build_dir = f'_build/automation/{instance}'
    ctx.build_dir = build_dir
    instance_command(ctx, instance, Command.FLASH)

@task()
def log(ctx, instance):
    """Show a real-time log output for an automation instance"""
    instance_command(ctx, instance, Command.LOG)

@task()
def get_test_selection(ctx, message="", files="", run_everything=False):
    # Get the instance DATABASE by parsing the data file
    db_data = u_data.get(DATABASE)
    instances = []
    filter_string = ""
    files = files.split(" ")

    if run_everything:
        # Safety switch has been thrown, run the lot
        print(f"run_everything flag set - running everything")
        instances = u_data.get_instances_all(db_data)
    else:
        # Parse the message
        found, filter_string = commit_message_parse(message, instances)
        if found:
            if instances and instances[0][0] == "*":
                # If there is a user instance, do what we're told
                print("running everything ", end="")
                if filter_string:
                    print(f"on API \"{filter_string}\" ", end="")
                print("at user request.")
                del instances[:]
                instances = u_data.get_instances_all(db_data)
        else:
            # No instance specified by the user, decide what to run
            filter_string = u_select.select(db_data, instances, files)

    instance_entries = []
    for id in instances:
        instance_entries.append({
            "id": id,
            "platform": u_data.get_platform_for_instance(db_data, id),
            "description": u_data.get_description_for_instance(db_data, id),
            "mcu": u_data.get_mcu_for_instance(db_data, id),
        })

    json_data = json.dumps({
        "filter": filter_string,
        "instances": instance_entries
    })

    print("JSON_DATA: " + json_data)
