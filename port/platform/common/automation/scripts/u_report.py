#!/usr/bin/env python

'''Write a report on the outcome of testing.'''
from time import gmtime, strftime, sleep
import threading                  # For ReportThread
from queue import Empty           # For ReportThread
import logging
import verboselogs
from scripts import u_utils
from scripts.u_logging import ULog

# Prefix to put at the start of all prints
PROMPT = "u_report"

# The event types
EVENT_TYPE_CHECK = "check"
EVENT_TYPE_BUILD = "build"
EVENT_TYPE_DOWNLOAD = "download"
EVENT_TYPE_BUILD_DOWNLOAD = "build/download"
EVENT_TYPE_TEST = "test"
EVENT_TYPE_INFRASTRUCTURE = "infrastructure"

class Event:
    def __init__(self, string, level=verboselogs.NOTICE):
        self._string = string
        self._level = level

    def __str__(self):
        return self._string

    def __eq__(self, other):
        return self._string == other

    def get_log_level(self):
        return self._level

class Ansi:
    LIGHT_GREEN = "\033[0;32m"
    LIGHT_CYAN = "\033[1;36m"
    END = "\033[0m"

# Events
EVENT_START =       Event("start")
EVENT_COMPLETE =    Event("complete")
EVENT_FAILED =      Event("*** FAILED ***",  logging.ERROR)
EVENT_PASSED =      Event("PASSED", verboselogs.SUCCESS)
EVENT_NAME =        Event("name")
EVENT_INFORMATION = Event("information")
EVENT_WARNING =     Event("*** WARNING ***", logging.WARNING)
EVENT_ERROR =       Event("*** ERROR ***",   logging.ERROR)

# Events that are used by tests only
EVENT_TEST_SUITE_COMPLETED = Event("suite completed")

# Internal event types and events
EVENT_TYPE_INTERNAL = "report"
EVENT_INTERNAL_OPEN =  Event("open")
EVENT_INTERNAL_CLOSE = Event("close")
EVENT_INTERNAL_EXTRA_INFORMATION = Event("info")

def event_as_string(event):
    '''Return a string version of an event'''
    need_comma = False
    if (event["type"] == EVENT_TYPE_INTERNAL) and               \
       (event["event"] == EVENT_INTERNAL_EXTRA_INFORMATION):
        string = str(event["event"])
    else:
        string = event["type"] + " " + str(event["event"])
    if "supplementary" in event:
        if event["event"] == EVENT_NAME:
            string += " " + event["supplementary"]
        else:
            string += " (" + event["supplementary"] + ")"
    if "extra_information" in event:
        string += ": " + str(event["extra_information"])
    if (event["type"] == EVENT_TYPE_TEST) and             \
       (event["event"] == EVENT_TEST_SUITE_COMPLETED):
        string += ": "
        if "tests_run" in event and (event["tests_run"] is not None):
            string += f"{event['tests_run']} run"
            need_comma = True
        if "tests_failed" in event and (event["tests_failed"] is not None):
            if need_comma:
                string += ", "
            if event["tests_failed"] > 0:
                string += f"{event['tests_failed']} {EVENT_FAILED}"
            else:
                string += f"{event['tests_failed']} failed"
            need_comma = True
        if "tests_ignored" in event and (event["tests_ignored"] is not None):
            if need_comma:
                string += ", "
            string += f"{event['tests_ignored']} ignored"

    return string

class ReportThread(threading.Thread):
    '''Reporting thread so that multiple processes can report at once'''
    def __init__(self, queue, file_handle):
        self._queue = queue
        self._running = False
        self._file_handle = file_handle
        self._events = []
        threading.Thread.__init__(self)
    def _write_events(self, instance):
        '''Write the events for an instance to file'''
        remove_list = []
        for event in self._events:
            if event["instance"] == instance:
                remove_list.append(event)
                if self._file_handle:
                    string = strftime(u_utils.TIME_FORMAT, event["timestamp"]) +  \
                             " instance " + u_utils.get_instance_text(instance) + \
                             " " + event_as_string(event) + ".\n"
                    self._file_handle.write(string)
        if self._file_handle:
            self._file_handle.flush()
        # Remove the list items we've written
        for item in remove_list:
            self._events.remove(item)
    def stop_thread(self):
        '''Helper function to stop the thread'''
        self._running = False
    def add_event(self, event):
        '''Add the event to the report'''
        if event["instance"] is None:
            # If the instance is None, this is the top-level
            # summary entity so print it straight away
            if self._file_handle:
                string = strftime(u_utils.TIME_FORMAT, event["timestamp"]) +  \
                         " " + event_as_string(event) + ".\n"
                self._file_handle.write(string)
        else:
            # If the event is not a boring one, add it to the log
            if (event["type"] != EVENT_TYPE_INTERNAL) or            \
               (event["event"] == EVENT_INTERNAL_EXTRA_INFORMATION):
                self._events.append(event)
            # If this is a close event, write the events
            # for this instance to file
            if (event["type"] == EVENT_TYPE_INTERNAL) and   \
               (event["event"] == EVENT_INTERNAL_CLOSE):
                self._write_events(event["instance"])
    def run(self):
        '''Worker thread'''
        self._running = True
        while self._running:
            try:
                event = self._queue.get(block=False, timeout=0.5)
                self.add_event(event)
            except Empty:
                sleep(0.1)

class ReportToQueue():
    '''Write a report to a queue, if there is one'''
    def __init__(self, queue, instance, file_handle, ):
        self._queue = queue
        self._instance = instance
        self._file_handle = file_handle
        prompt = PROMPT
        if instance:
            prompt += "_" + u_utils.get_instance_text(instance)
        self._logger = ULog.get_logger(prompt)
    def __enter__(self):
        # Send an open event for this instance
        event = {}
        event["type"] = EVENT_TYPE_INTERNAL
        event["event"] = EVENT_INTERNAL_OPEN
        event["instance"] = self._instance
        self._send(event)
        return self
    def __exit__(self, _type, value, traceback):
        del _type
        del value
        del traceback
        # Send a close event
        event = {}
        event["type"] = EVENT_TYPE_INTERNAL
        event["event"] = EVENT_INTERNAL_CLOSE
        self._send(event)
    def _send(self, event):
        '''Send an event to the reporting queue'''
        event["timestamp"] = gmtime()
        event["instance"] = self._instance
        if self._queue:
            self._queue.put(event.copy())

        string = event_as_string(event)
        if (event["type"] != EVENT_TYPE_INTERNAL or
            event["event"] != EVENT_INTERNAL_EXTRA_INFORMATION):
            level = event["event"].get_log_level()
            self._logger.log(level, string)

        string = strftime(u_utils.TIME_FORMAT, event["timestamp"]) + " " + string + ".\n"
        if self._file_handle:
            self._file_handle.write(string)
            self._file_handle.flush()
    def open(self):
        '''Send an open event, not required if used as with()'''
        event = {}
        event["type"] = EVENT_TYPE_INTERNAL
        event["event"] = EVENT_INTERNAL_OPEN
        event["instance"] = self._instance
        self._send(event)
    def close(self):
        '''Send a close event, not required if used as with()'''
        event = {}
        event["type"] = EVENT_TYPE_INTERNAL
        event["event"] = EVENT_INTERNAL_CLOSE
        self._send(event)
    def event(self, event_type, event, supplementary=None):
        '''Report a generic event'''
        _event = {}
        _event["type"] = event_type
        _event["event"] = event
        if supplementary:
            _event["supplementary"] = supplementary
        self._send(_event)
    def event_extra_information(self, extra_information):
        '''Report a generic event'''
        event = {}
        event["type"] = EVENT_TYPE_INTERNAL
        event["event"] = EVENT_INTERNAL_EXTRA_INFORMATION
        event["extra_information"] = extra_information
        self._send(event)
    def test_suite_completed_event(self, run, failed, ignored, supplementary=None):
        '''Report the completion of a test suite, with pass/fail/skip values'''
        event = {}
        event["type"] = EVENT_TYPE_TEST
        event["event"] = EVENT_TEST_SUITE_COMPLETED
        if supplementary:
            event["supplementary"] = supplementary
        event["tests_run"] = run
        event["tests_failed"] = failed
        event["tests_ignored"] = ignored
        self._send(event)
