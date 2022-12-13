#!/usr/bin/env python

'''Perform a Doxygen check on ubxlib report results.'''

import os          # For sep
import subprocess
from logging import Logger
from scripts import u_report, u_utils, u_settings
from scripts.u_logging import ULog

# Prefix to put at the start of all prints
PROMPT = "u_run_doxygen"

# The logger
U_LOG: Logger = None

# The name of the Doxygen configuration file to use
DOXYFILE = u_settings.DOXYGEN_DOXYFILE #"Doxyfile"

def run(ubxlib_dir, reporter):
    '''Run Doxygen'''
    return_value = 1
    got_doxygen = False

    # "global" should be avoided, but we make an exception for the logger
    global U_LOG # pylint: disable=global-statement
    U_LOG = ULog.get_logger(PROMPT)

    working_dir = os.getcwd()

    # Print out what we've been told to do
    text = "running Doxygen from ubxlib directory \"" + ubxlib_dir + "\""
    U_LOG.info(text)

    reporter.event(u_report.EVENT_TYPE_CHECK,
                   u_report.EVENT_START,
                   "Doxygen")

    got_doxygen = u_utils.exe_where("doxygen", \
                                    "ERROR: can't find Doxygen, please make"     \
                                    " sure that it is installed and on the path.", \
                                    logger=U_LOG)

    if got_doxygen:
        # Sort out any subst
        U_LOG.info("Doxygen finds no files if run from a aubst drive to" \
                   f" convert {ubxlib_dir} to a real path")
        actual_ubxlib_dir = u_utils.get_actual_path(ubxlib_dir)
        U_LOG.info(f"Actual ubxlib directory is {actual_ubxlib_dir}")
        # Run Doxygen
        config_path = actual_ubxlib_dir + os.sep + DOXYFILE
        U_LOG.info(f"CD to {actual_ubxlib_dir}...")
        with u_utils.ChangeDir(actual_ubxlib_dir):
            U_LOG.info("in directory {} calling doxygen {}.".
                       format(os.getcwd(), config_path))
            try:
                my_env = os.environ.copy()
                my_env['UBX_WORKDIR'] = working_dir
                proc = subprocess.run(["doxygen", config_path],
                                      capture_output=True,
                                      check=True,
                                      env=my_env, shell=True)

                reporter.event(u_report.EVENT_TYPE_CHECK,
                               u_report.EVENT_PASSED)
                for line in proc.stdout.splitlines():
                    U_LOG.info(line.decode())
                for line in proc.stderr.splitlines():
                    U_LOG.info(line.decode())
                return_value = 0
            except subprocess.CalledProcessError as error:
                reporter.event(u_report.EVENT_TYPE_CHECK,
                               u_report.EVENT_FAILED)
                for line in error.output.splitlines():
                    U_LOG.info(line.decode())
                U_LOG.error(f"Doxygen returned error {error.returncode}:")
                for line in error.stderr.splitlines():
                    line = line.rstrip().decode()
                    if line:
                        reporter.event_extra_information(line)
                        U_LOG.error(line)
    else:
        reporter.event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                       u_report.EVENT_FAILED,
                       "there is a problem with the Doxygen installation")

    return return_value
