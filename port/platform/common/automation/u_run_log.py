#!/usr/bin/env python

'''Test result logger for external HW.'''

import u_monitor
import u_report
import u_utils

# Prefix to put at the start of all prints
PROMPT = "u_run_log_"

# The guard time for running tests in seconds
RUN_GUARD_TIME_SECONDS = u_utils.RUN_GUARD_TIME_SECONDS

# The inactivity time for running tests in seconds
RUN_INACTIVITY_TIME_SECONDS = u_utils.RUN_INACTIVITY_TIME_SECONDS

def run(instance, printer, reporter, test_report_handle, keep_going_flag=None):
    '''Log test result of external HW'''
    return_value = -1
    instance_text = u_utils.get_instance_text(instance)
    prompt = PROMPT + instance_text + ": "
    cmd = ["inv", "-r", f"{u_utils.UBXLIB_DIR}/.vscode", "automation.log", instance_text]

    reporter.event(u_report.EVENT_TYPE_TEST,
                   u_report.EVENT_START)

    with u_utils.ExeRun(cmd, printer, prompt) as process:
        return_value = u_monitor.main(process,
                                      u_monitor.CONNECTION_PROCESS,
                                      RUN_GUARD_TIME_SECONDS,
                                      RUN_INACTIVITY_TIME_SECONDS,
                                      None, instance, printer,
                                      reporter,
                                      test_report_handle,
                                      keep_going_flag=keep_going_flag)
        if return_value == 0:
            reporter.event(u_report.EVENT_TYPE_TEST,
                           u_report.EVENT_COMPLETE)
        else:
            reporter.event(u_report.EVENT_TYPE_TEST,
                           u_report.EVENT_FAILED)

    return return_value
