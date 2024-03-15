#!/usr/bin/env python

'''Build/run ubxlib for Linux and report results.'''
import os                    # For sep(), getcwd(), listdir()
from logging import Logger
from tasks import nrfconnect, linux
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

# The device name that pppd will connect (i.e. U_PORT_PPP_LOCAL_SOCKET_NAME)
PPP_LOCAL_SOCKET_NAME = "127.0.0.1:5000"

# The start marker to use on a Valgrind print
VALGRIND_START_MARKER = "VALGRIND SAYS:"

# Regex to capture the start of a block of Valgrind output
VALGRIND_REGEX = f"^==\\d+== {VALGRIND_START_MARKER}$"

# The path to the valgrind suppression file
VALGRIND_SUPPRESSION_PATH = f"{u_utils.UBXLIB_DIR}/port/platform/linux/valgrind.supp"

# Used by native Linux when Valgrind is employed.
# Note: valgrind_error_counter has to be a mutable object,
# in this case a list, so that the callback can update it.
def valgrind_callback(match, valgrind_error_counter, results, reporter):
    '''Count up and highlight Valgrind issues'''
    del match
    del results
    valgrind_error_counter[0] += 1
    if reporter:
        reporter.event(u_report.EVENT_TYPE_TEST,
                       u_report.EVENT_ERROR,
                       "possible memory problem flagged by Valgrind")

def uart_to_device_list_create(u_flags, logger):
    '''Create a UART context by parsing u_flags'''
    uart_to_device_list = []
    u_cfg_test_uart_prefix = ""
    u_cfg_app_uart_prefix = ""

    # Parse u_flags to find values for the following:
    # U_CFG_TEST_UART_A: if present, create an object in the
    # uart_to_device array of type "U_CFG_TEST_UART_A" and
    # set the value of "uart" to this
    # U_CFG_TEST_UART_B: as for U_CFG_TEST_UART_A
    # U_CFG_APP_xxxx_UART: for each value present, find the
    # corresponding U_CFG_APP_xxxx_UART_DEV and create an
    # object in uart_to_device list for each one, including
    # the type, the UART number and the device_to, e.g.:
    #
    # {"type": "U_CFG_APP_CELL_UART", "uart": "1", "device_to": "/dev/tty/3"}

    for flag in u_flags:
        if flag.startswith("U_CFG_TEST_UART_PREFIX"):
            parts = flag.split("=")
            if parts and len(parts) > 1:
                u_cfg_test_uart_prefix = parts[1]
        elif flag.startswith("U_CFG_APP_UART_PREFIX"):
            parts = flag.split("=")
            if parts and len(parts) > 1:
                u_cfg_app_uart_prefix = parts[1]
        elif flag.startswith("U_CFG_TEST_UART_A") or flag.startswith("U_CFG_TEST_UART_B"):
            parts = flag.split("=")
            if parts and len(parts) > 1:
                uart_to_device = {}
                uart_to_device["type"] = parts[0]
                uart_to_device["uart"] = u_cfg_test_uart_prefix + parts[1]
                uart_to_device_list.append(uart_to_device)
                logger.info(uart_to_device["type"] + ': will be UART ' + \
                            uart_to_device["uart"])
        elif flag.startswith("U_CFG_APP_") and "_UART=" in flag:
            parts = flag.split("=")
            if parts and len(parts) > 1:
                uart_to_device = {}
                uart_to_device["type"] = parts[0]
                uart_to_device["uart"] = u_cfg_app_uart_prefix + parts[1]
                uart_to_device_list.append(uart_to_device)
        elif flag.startswith("U_CFG_APP_") and "_UART_DEV=" in flag:
            parts = flag.split("=")
            if parts and len(parts) > 1:
                uart_type = parts[0].split("_DEV")[0]
                for uart_to_device in uart_to_device_list:
                    if uart_to_device["type"] == uart_type:
                        uart_to_device["device_to"] = parts[1]
                        logger.info(uart_to_device["type"] + ': will be UART ' + \
                                    uart_to_device["uart"] + ' to ' + uart_to_device["device_to"])
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

# This used by Zephyr-Linux, called-back by u_monitor.py once
# it has captured the random UART-device-mappings that a
# Zephyr-Linux executable tells us it has chosen.
def uart_to_device_callback(match, uart_to_device_list, results, reporter):
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
                    # Note: neither of these devices need to be created as PTYs
                    # with a link since the Zephyr/Linux executable will have
                    # created them
                    DEVICE_REDIRECTS.append(u_utils.device_redirect_start(device_a, False,
                                                                          device_b, False,
                                                                          UART_BAUD_RATE))
                if reporter and message:
                    reporter.event(u_report.EVENT_TYPE_TEST,
                                   u_report.EVENT_INFORMATION,
                                   message)
            break

# This used by native Linux to implement fixed UART redirections
# or UART loopbacks, all of which are dictated entirely by
# DATABASE.md, the executable doesn't get to choose.
def redirect_uart_fixed(uart_to_device_list, reporter):
    '''Set up fixed UART redirects'''
    for uart_to_device in uart_to_device_list:
        if "done" not in uart_to_device:
            message = ""
            device_a = ""
            device_b = ""
            to_pty = False
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
                    if "done" not in other_end:
                        # Have both ends and the loop-back has
                        # not been done so do it now
                        message = uart_to_device["type"] + "/" + other_end["type"] + \
                                  ": " + uart_to_device["uart"] + \
                                  " will be looped back to " + \
                                  other_end["uart"] + ")"
                        device_a = uart_to_device["uart"]
                        device_b = other_end["uart"]
                        # Need  these to be PTYs, i.e. virtual devices we will create
                        to_pty = True
                        # Mark both as done
                        other_end["done"] = True
                        uart_to_device["done"] = True
                else:
                    # Don't have the other end in our list so if this
                    # is the UART of U_CFG_TEST_UART_A then loop it back
                    # on itself
                    if uart_to_device["type"] == "U_CFG_TEST_UART_A":
                        message = uart_to_device["type"] + ": " + \
                                  uart_to_device["uart"] + " will be looped back on itself"
                        device_a = uart_to_device["uart"]
                        device_b = uart_to_device["uart"]
                        # Mark it as done
                        uart_to_device["done"] = True
            else:
                # This is not a looped-back one, it is a simple
                # forwarding case
                message = uart_to_device["type"] + ": " + \
                          uart_to_device["uart"] + " will be redirected to " + \
                          uart_to_device["device_to"]
                device_a = uart_to_device["uart"]
                device_b = uart_to_device["device_to"]
                # Mark it as done
                uart_to_device["done"] = True
            if device_a and device_b:
                # Actually do it
                # The first device is always a PTY as we need to create it, the
                # second is a PTY only for the loop-back case, where we have
                # to create it, and not if we're redirecting to a real device
                DEVICE_REDIRECTS.append(u_utils.device_redirect_start(device_a, True,
                                                                      device_b, to_pty,
                                                                      UART_BAUD_RATE))
            if reporter and message:
                reporter.event(u_report.EVENT_TYPE_TEST,
                               u_report.EVENT_INFORMATION,
                               message)

def run(ctx, instance, platform, board_name=DEFAULT_BOARD_NAME, build_dir=DEFAULT_BUILD_DIR,
        output_name=DEFAULT_OUTPUT_NAME, defines=None, connection=None, connection_lock=None,
        features=None):
    '''Build/run on Linux'''
    return_value = -1
    instance_text = u_utils.get_instance_text(instance)
    valgrind_error_counter = [0]

    # "global" should be avoided, but we make an exception for the logger
    global U_LOG # pylint: disable=global-statement
    U_LOG = ULog.get_logger(PROMPT + instance_text)

    # Linux beneath Zephyr
    if platform.lower() == "zephyr":
        ctx.reporter.event(u_report.EVENT_TYPE_BUILD,
                           u_report.EVENT_START,
                           "Linux")
        # Perform the build
        exe_path = os.path.abspath(os.path.join(build_dir, "zephyr", "zephyr.exe"))
        if os.path.isfile(exe_path):
            os.remove(exe_path)
        nrfconnect.build(ctx, cmake_dir=DEFAULT_CMAKE_DIR, board_name=board_name,
                         output_name=output_name, build_dir=build_dir,
                         u_flags=defines, features=features)
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
                        u_monitor.callback(uart_to_device_callback, UART_TO_DEVICE_REGEX, uart_to_device_list)

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
    # Native Linux
    elif platform.lower() == "linux":
        ctx.reporter.event(u_report.EVENT_TYPE_BUILD,
                           u_report.EVENT_START,
                           "Linux")
        # Perform the build
        exe_path = os.path.abspath(os.path.join(build_dir, "runner", "ubxlib_test_main"))
        if os.path.isfile(exe_path):
            os.remove(exe_path)
        linux.build(ctx, cmake_dir=f"{u_utils.UBXLIB_DIR}/port/platform/linux/mcu/posix/runner",
                    output_name="runner", build_dir=build_dir, u_flags=defines,
                    features=features)
        # Build has succeeded, we should have an executable
        if os.path.isfile(exe_path):
            # Lock the connection in order to run
            with u_connection.Lock(connection, connection_lock,
                                   CONNECTION_LOCK_GUARD_TIME_SECONDS,
                                   logger=U_LOG) as locked_connection:
                if locked_connection:
                    # Get pppd running, in case we're testing PPP
                    cmd = ["pppd", "socket", f"{PPP_LOCAL_SOCKET_NAME}", f"{UART_BAUD_RATE}",
                           "passive", "persist", "maxfail", "0", "local", "defaultroute"]
                    with u_utils.ExeRun(cmd, logger=U_LOG) as process:
                        # Create the UART loopbacks/redirections as directed by the list of defines
                        #
                        # For instance (noting NO quotation marks in the values of the defines):
                        #
                        # U_CFG_TEST_UART_PREFIX=/tmp/ttyv U_CFG_TEST_UART_A=0
                        #
                        # ...would cause "/tmp/ttyv0" to be looped-back on itself, or:
                        #
                        # U_CFG_TEST_UART_PREFIX=/tmp/ttyv U_CFG_TEST_UART_A=0 U_CFG_TEST_UART_B=1
                        #
                        # ...would cause "/tmp/ttyv0" to be looped-back to "/tmp/ttyv1", or:
                        #
                        # U_CFG_TEST_APP_PREFIX=/dev/tty U_CFG_APP_CELL_UART=0 U_CFG_APP_CELL_UART_DEV=2
                        #
                        # ...would cause the cellular UART "/dev/tty0" to be redirected to "/dev/tty2"
                        uart_to_device_list = uart_to_device_list_create(defines, logger=U_LOG)
                        if uart_to_device_list:
                            redirect_uart_fixed(uart_to_device_list, ctx.reporter)
                        # Start the executable and monitor what it spits out
                        call_list = [exe_path]
                        if "U_CFG_TEST_USE_VALGRIND" in defines:
                            call_list = ["valgrind"] + ["--leak-check=yes"] +          \
                                        [f"--error-markers={VALGRIND_START_MARKER}"] + \
                                        [f"--suppressions={VALGRIND_SUPPRESSION_PATH}"] + \
                                        call_list

                            # If you need Valgrind to suppress more errors, the best way
                            # to go about that is to temporarily use the following line
                            # to obtain the necessary suppression-file contents in the
                            # log output.  You can then use that as a basis for adding
                            # the correct suppressions to VALGRIND_SUPPRESSION_PATH
                            #
                            # call_list = ["valgrind"] + ["--leak-check=yes"] + \
                            #             [f"--error-markers={VALGRIND_START_MARKER}"] + \
                            #             ["--gen-suppressions=all"] +          \
                            #             [f"--suppressions={VALGRIND_SUPPRESSION_PATH}"] + call_list
                            #
                            # Note: the Valgrind leak summary is emitted after the executable
                            # has been sent SIGINT, at which point we are no longer monitoring
                            # its output, hence no leak error will be flagged by this script
                            # (though they will still be there in the logged output)
                            u_monitor.callback(valgrind_callback, VALGRIND_REGEX,
                                               valgrind_error_counter)
                        with u_utils.ExeRun(call_list, logger=U_LOG) as process:
                            return_value = u_monitor.main(process,
                                                          u_monitor.CONNECTION_PROCESS,
                                                          RUN_GUARD_TIME_SECONDS,
                                                          RUN_INACTIVITY_TIME_SECONDS,
                                                          None, instance,
                                                          ctx.reporter,
                                                          ctx.test_report)
                            return_value += valgrind_error_counter[0]
                            if return_value == 0:
                                ctx.reporter.event(u_report.EVENT_TYPE_TEST,
                                                   u_report.EVENT_COMPLETE)
                            else:
                                ctx.reporter.event(u_report.EVENT_TYPE_TEST,
                                                   u_report.EVENT_FAILED)
                        # Remove the redirections
                        for device_redirect in DEVICE_REDIRECTS:
                            u_utils.device_redirect_stop(device_redirect)
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
