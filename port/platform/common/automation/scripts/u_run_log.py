#!/usr/bin/env python

'''Test result logger for external HW.'''

from logging import Logger
from scripts import u_monitor, u_report, u_utils
from scripts.u_logging import ULog

# Prefix to put at the start of all prints
PROMPT = "u_run_log_"

# The logger
U_LOG: Logger = None

# The guard time for running tests in seconds
RUN_GUARD_TIME_SECONDS = u_utils.RUN_GUARD_TIME_SECONDS

# The inactivity time for running tests in seconds
RUN_INACTIVITY_TIME_SECONDS = u_utils.RUN_INACTIVITY_TIME_SECONDS

def run(instance, build_dir, reporter, test_report_file_path):
    '''Log test result of external HW'''
    return_value = -1
    instance_text = u_utils.get_instance_text(instance)

    # "global" should be avoided, but we make an exception for the logger
    global U_LOG # pylint: disable=global-statement
    U_LOG = ULog.get_logger(PROMPT + instance_text)

    cmd = [
        "inv", "-r", f"{u_utils.AUTOMATION_DIR}",
        "automation.log", f"--build-dir={build_dir}", instance_text
    ]

    reporter.event(u_report.EVENT_TYPE_TEST,
                   u_report.EVENT_START)

    with u_utils.ExeRun(cmd, logger=U_LOG) as process:
        return_value = u_monitor.main(process,
                                      u_monitor.CONNECTION_PROCESS,
                                      RUN_GUARD_TIME_SECONDS,
                                      RUN_INACTIVITY_TIME_SECONDS,
                                      None, instance,
                                      reporter,
                                      test_report_file_path)
        if return_value == 0:
            reporter.event(u_report.EVENT_TYPE_TEST,
                           u_report.EVENT_COMPLETE)
        else:
            reporter.event(u_report.EVENT_TYPE_TEST,
                           u_report.EVENT_FAILED)

    return return_value
