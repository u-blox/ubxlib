#!/usr/bin/env python

'''Build/run ubxlib for Linux and report results.'''
import os                    # For sep(), getcwd(), listdir()
from logging import Logger
from tasks import nrfconnect
from scripts import u_connection, u_monitor, u_report, u_utils
from scripts.u_logging import ULog

# Prefix to put at the start of all prints
PROMPT = "u_run_linux_"

# The logger
U_LOG: Logger = None

DEFAULT_CMAKE_DIR = f"{u_utils.UBXLIB_DIR}/port/platform/zephyr/runner_linux"
DEFAULT_BOARD_NAME = "native_posix"
DEFAULT_OUTPUT_NAME = f"runner_{DEFAULT_BOARD_NAME}"
DEFAULT_BUILD_DIR = os.path.join("_build", "nrfconnect")

# The guard time waiting for a lock on the HW connection seconds
CONNECTION_LOCK_GUARD_TIME_SECONDS = u_connection.CONNECTION_LOCK_GUARD_TIME_SECONDS

# The guard time for running tests in seconds
RUN_GUARD_TIME_SECONDS = u_utils.RUN_GUARD_TIME_SECONDS

# The inactivity time for running tests in seconds
RUN_INACTIVITY_TIME_SECONDS = u_utils.RUN_INACTIVITY_TIME_SECONDS

# Regex to capture a string of the form:
#
# UART_1 connected to pseudotty: /dev/pts/5
#
# ...where it would return "1" in the first capture group and "/dev/pts/5"
# in the second capture group. This tested on https://regex101.com/ with
# Python the selected regex flavour.
UART_TO_DEVICE_REGEX = r"(?:^UART_([0-9]*) connected to pseudotty: (.*?))$"

# List of device redirections that need to be terminated when done
DEVICE_REDIRECTS = []

# The baud rate to use: this is necessary since the redirection process
# doesn't seem to work for real TTYs unless it is given as a parameter
# to the socat utility which does the redirection
UART_BAUD_RATE = 115200

def uart_to_device_list_create(u_flags, logger):
    '''Create a UART context by parsing u_flags'''
    uart_to_device_list = []

    # Parse u_flags to find values for the following:
    # U_CFG_TEST_UART_A: if present, create an object in the
    # uart_to_device array of type "U_CFG_TEST_UART_A" and
    # set the value of "uart" to this
    # U_CFG_TEST_UART_B: as for U_CFG_TEST_UART_A
    # U_CFG_APP_xxxx_UART: for each value present, find the
    # corresponding U_CFG_APP_xxxx_UART_DEV and create an
    # object in uart_to_device list for each one, including
    # the type, the UART number and the device, e.g.:
    #
    # {"type": "U_CFG_APP_CELL_UART", "uart": "1", "device": "/dev/tty/3"}

    for flag in u_flags:
        if flag.startswith("U_CFG_TEST_UART_A") or flag.startswith("U_CFG_TEST_UART_B"):
            parts = flag.split("=")
            if parts and len(parts) > 1:
                uart_to_device = {}
                uart_to_device["type"] = parts[0]
                uart_to_device["uart"] = parts[1]
                uart_to_device_list.append(uart_to_device)
                logger.info(uart_to_device["type"] + f': will redirect UART_' + \
                            uart_to_device["uart"])
        elif flag.startswith("U_CFG_APP_") and "_UART=" in flag:
            parts = flag.split("=")
            if parts and len(parts) > 1:
                uart_to_device = {}
                uart_to_device["type"] = parts[0]
                uart_to_device["uart"] = parts[1]
                uart_to_device_list.append(uart_to_device)
        elif flag.startswith("U_CFG_APP_") and "_UART_DEV=" in flag:
            parts = flag.split("=")
            if parts and len(parts) > 1:
                uart_type = parts[0].split("_DEV")[0]
                for uart_to_device in uart_to_device_list:
                    if uart_to_device["type"] == uart_type:
                        uart_to_device["device_to"] = parts[1]
                        logger.info(uart_to_device["type"] + f': redirecting UART_' + \
                                    uart_to_device["uart"] + f' to ' + uart_to_device["device_to"])
                        break
    # When done, check if there were any U_CFG_APP_xxx_UART
    # entries without a corresponding U_CFG_APP_xxx_UART_DEV entry
    # and delete them
    delete_list = []
    for idx, uart_to_device in enumerate(uart_to_device_list):
        flag = uart_to_device["type"]
        if flag.startswith("U_CFG_APP_") and flag.endswith("_UART") and \
           "device_to" not in uart_to_device:
            delete_list.append(idx)
    # Remove the largest index items first so as not to change
    # the order of the list as we go
    delete_list.sort(reverse=True)
    for item in delete_list:
        uart_to_device_list.pop(item)

    return uart_to_device_list

def callback(match, uart_to_device_list, results, reporter):
    '''Redirect a UART based on the match and the list of UARTs we care about'''

    del results

    # match group 1 will contain the UART number and match group 2
    # the device it appears on.  Need to search through the
    # UART device list to determine what to do with them
    # Note that the following are all string compares, which
    # should be fine
    uart_number = match.group(1)
    device_from = match.group(2)
    for uart_to_device in uart_to_device_list:
        if uart_to_device["uart"] == uart_number:
            if "done" not in uart_to_device:
                # This is a UART number for which we have a mapping that's not
                # been done. First store the device_from string
                uart_to_device["device_from"] = device_from
                message = ""
                device_a = ""
                device_b = ""
                if uart_to_device["type"].startswith("U_CFG_TEST_UART_"):
                    # This is one we want to loop back: see if we have
                    # the other UART loopback entry in the list also
                    other_end = {}
                    wanted = "_B"
                    if uart_to_device["type"].endswith("_B"):
                        wanted = "_A"
                    for temp in uart_to_device_list:
                        if temp["type"].endswith(wanted):
                            other_end = temp
                            break
                    if other_end:
                        if "device_from" in other_end and "done" not in other_end:
                            # Have both ends and the loop-back has
                            # not been done so do it now
                            message = uart_to_device["type"] + "/" + other_end["type"] + \
                                      ": " + uart_to_device["device_from"] + \
                                      " (UART_" + uart_to_device["uart"] + \
                                      ") will be looped back to " + \
                                      other_end["device_from"] + " (UART_" + \
                                      other_end["uart"] + ")"
                            device_a = uart_to_device["device_from"]
                            device_b = other_end["device_from"]
                            # Mark both as done
                            other_end["done"] = True
                            uart_to_device["done"] = True
                    else:
                        # Don't have the other end in our list so if this
                        # is the UART of U_CFG_TEST_UART_A then loop it back
                        # on itself
                        if uart_to_device["type"] == "U_CFG_TEST_UART_A":
                            message = uart_to_device["type"] + ": " + \
                                      uart_to_device["device_from"] + " (UART_" + \
                                      uart_to_device["uart"] + ") will be looped back on itself"
                            device_a = uart_to_device["device_from"]
                            device_b = uart_to_device["device_from"]
                            # Mark it as done
                            uart_to_device["done"] = True
                else:
                    # This is not a looped-back one, it is a simple
                    # forwarding case
                    message = uart_to_device["type"] + ": " + \
                              uart_to_device["device_from"] + " (UART_" + \
                              uart_to_device["uart"] + ") will be redirected to " + \
                              uart_to_device["device_to"]
                    device_a = uart_to_device["device_from"]
                    device_b = uart_to_device["device_to"]
                    # Mark it as done
                    uart_to_device["done"] = True
                if device_a and device_b:
                    # Actually do it
                    DEVICE_REDIRECTS.append(u_utils.device_redirect_start(device_a, device_b, UART_BAUD_RATE))
                if reporter and message:
                    reporter.event(u_report.EVENT_TYPE_TEST,
                                   u_report.EVENT_INFORMATION,
                                   message)
            break

def run(ctx, instance, platform, board_name=DEFAULT_BOARD_NAME, build_dir=DEFAULT_BUILD_DIR,
        output_name=DEFAULT_OUTPUT_NAME, defines=None, connection=None, connection_lock=None):
    '''Build/run on Linux'''
    return_value = -1
    instance_text = u_utils.get_instance_text(instance)

    # "global" should be avoided, but we make an exception for the logger
    global U_LOG # pylint: disable=global-statement
    U_LOG = ULog.get_logger(PROMPT + instance_text)

    # Currently only support Linux beneath Zephyr
    if platform.lower() == "zephyr":
        ctx.reporter.event(u_report.EVENT_TYPE_BUILD,
                           u_report.EVENT_START,
                           "Linux")
        # Perform the build
        exe_path = os.path.abspath(os.path.join(build_dir, "zephyr", "zephyr.exe"))
        if os.path.isfile(exe_path):
            os.remove(exe_path)
        nrfconnect.build(ctx, cmake_dir=DEFAULT_CMAKE_DIR, board_name=board_name,
                         output_name=output_name, build_dir=build_dir, u_flags=defines)
        # Build has succeeded, we should have an executable
        if os.path.isfile(exe_path):
            # Lock the connection in order to run
            with u_connection.Lock(connection, connection_lock,
                                   CONNECTION_LOCK_GUARD_TIME_SECONDS,
                                   logger=U_LOG) as locked_connection:
                if locked_connection:
                    # For Linux-under-Zephyr we need to capture the first two
                    # lines that the Zephyr executable prints out, which will
                    # be of the following form:
                    #
                    # UART_1 connected to pseudotty: /dev/pts/5
                    # UART_0 connected to pseudotty: /dev/pts/3
                    #
                    # ...and redirect those devices either to loop-back(s) or
                    # to other devices, as defined by flags set in DATABASE.md.
                    uart_to_device_list = uart_to_device_list_create(defines, logger=U_LOG)
                    if uart_to_device_list:
                        u_monitor.callback(callback, UART_TO_DEVICE_REGEX, uart_to_device_list)

                    # Start the .exe and monitor what it spits out
                    try:
                        with u_utils.ExeRun([exe_path], logger=U_LOG) as process:
                            return_value = u_monitor.main(process,
                                                          u_monitor.CONNECTION_PROCESS,
                                                          RUN_GUARD_TIME_SECONDS,
                                                          RUN_INACTIVITY_TIME_SECONDS,
                                                          None, instance,
                                                          ctx.reporter,
                                                          ctx.test_report)
                            if return_value == 0:
                                ctx.reporter.event(u_report.EVENT_TYPE_TEST,
                                                   u_report.EVENT_COMPLETE)
                            else:
                                ctx.reporter.event(u_report.EVENT_TYPE_TEST,
                                                   u_report.EVENT_FAILED)
                        # Remove the redirections
                        for device_redirect in DEVICE_REDIRECTS:
                            u_utils.device_redirect_stop(device_redirect)
                    except KeyboardInterrupt as ex:
                        # Remove the redirections in case of CTRL-C
                        for device_redirect in DEVICE_REDIRECTS:
                            u_utils.device_redirect_stop(device_redirect)
                        raise KeyboardInterrupt from ex
                else:
                    ctx.reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                       u_report.EVENT_FAILED,
                                       "unable to lock a connection")
        else:
            return_value = 1
            ctx.reporter.event(u_report.EVENT_TYPE_BUILD,
                               u_report.EVENT_FAILED,
                               "check debug log for details")
    else:
        return_value = 1
        ctx.reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                           u_report.EVENT_FAILED,
                           "unsupported platform \"" + platform + "\"")

    return return_value
