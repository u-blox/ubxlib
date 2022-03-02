#!/usr/bin/env python

'''A test agent that communicates with a controller and runs test instances.'''

from time import sleep
import os # For sep and getcwd() and makedirs()
from multiprocessing import Manager, freeze_support # To launch u_run_blah.py instances
import multiprocessing.pool                         # Specific import for daemonic process dodge
import threading
import traceback
import u_data   # Access to the instance database
import u_connection # To initialise locks
import u_run    # Actually run stuff
import u_report # reporting
import u_utils  # utils
import u_settings

# Prefix to put at the start of all prints
PROMPT = "u_agent: "

# Prefix for the individual instance working directory
INSTANCE_DIR_PREFIX = ""

# The number of seconds at which to report what's still running
STILL_RUNNING_REPORT_SECONDS = 30

# The thread name for the agent context
AGENT_CONTEXT_THREAD_NAME = "agent_context"

# The manager for the CONTEXT_LOCK
CONTEXT_MANAGER = None

# A lock to prevent context access clashing
CONTEXT_LOCK = None

# Flag so that we don't rise from the dead
AND_STAY_DEAD = False

# The ID strings of the USB cutters for this agent which should be cut
# and then un-cut as a global hw_reset when the agent is started.
USB_CUTTER_ID_STRS = u_settings.USB_CUTTER_ID_STRS # e.g. ["1750665"]

# These two wrapper classes stolen from:
# https://stackoverflow.com/questions/6974695/python-process-pool-non-daemonic
# Reason for it is that some of our pool processes need
# to be able to create processes themselves (e.g. for STM32F4)
# and processes that are marked as daemons, as they are
# by default, are not permitted to do that.
# Sub-class multiprocessing.pool.Pool instead of multiprocessing.Pool
# because the latter is only a wrapper function, not a proper class.
class NoDaemonProcess(multiprocessing.Process):
    '''make 'daemon' attribute always return False'''
    @property
    def daemon(self):
        return False

    @daemon.setter
    def daemon(self, val):
        pass

class NoDaemonPool(multiprocessing.pool.Pool):
    '''make 'daemon' attribute always return False'''
    def Process(self, *args, **kwds):
        proc = super().Process(*args, **kwds)
        proc.__class__ = NoDaemonProcess
        return proc

class Context(threading.Thread):
    '''Class to hold a context dictionary, with locks, as a thread'''
    def __init__(self):
        self._context = {}
        self._running = True
        threading.Thread.__init__(self)
    def __enter__(self):
        return self._context
    def __exit__(self, _type, value, traceback):
        del _type
        del value
        del traceback
    def __delitem__(self, key):
        del self._context[key]
    def __len__(self):
        value = len(self._context)
        return value
    def __contains__(self, key):
        value = False
        if key in self._context:
            value = True
        return value
    def __setitem__(self, key, value):
        self._context[key] = value
    def __getitem__(self, key):
        value = self._context[key]
        return value
    def run(self):
        while self._running:
            sleep(1)
    def stop_thread(self):
        '''Clear flag to make thread exit'''
        self._running = False

def create_platform_locks(database, instances, manager, platform_locks):
    '''Create a lock per platform in platform_locks'''
    for instance in instances:
        this_platform = u_data.get_platform_for_instance(database, instance)
        if this_platform:
            done = False
            for platform in platform_locks:
                if this_platform == platform["platform"]:
                    done = True
                    break
            if not done:
                platform_locks.append({"platform": this_platform, "lock": manager.RLock()})

def init():
    '''Create a manager to use for synchronisation: call this FIRST'''
    global CONTEXT_MANAGER
    if not CONTEXT_MANAGER:
        CONTEXT_MANAGER = Manager()

def deinit():
    '''Destroy the context manager: call this LAST'''
    global CONTEXT_MANAGER
    CONTEXT_MANAGER = None

def init_context_lock():
    '''Create the context lock, using the manager'''
    global CONTEXT_LOCK
    CONTEXT_LOCK = CONTEXT_MANAGER.RLock()

def deinit_context_lock():
    '''Destroy the context lock'''
    global CONTEXT_LOCK
    CONTEXT_LOCK = None

def get():
    '''Get the running agent'''
    found_thread = None
    for thread in threading.enumerate():
        if thread.name == AGENT_CONTEXT_THREAD_NAME:
            found_thread = thread
            break
    return found_thread

def start(print_queue=None, hw_reset=False):
    '''Start an agent and return true on success'''
    success = False
    if not AND_STAY_DEAD:
        with CONTEXT_LOCK:
            agent_context = get()
            if agent_context:
                success = True
            else:
                agent_context = Context()
                agent_context.name = AGENT_CONTEXT_THREAD_NAME
                agent_context.start()

                agent_context["manager"] = Manager()
                agent_context["misc_locks"] = {}

                # Create a lock to cover things that cross
                # platforms or that any process may need to
                # perform outside of its working directory
                agent_context["misc_locks"]["system_lock"] = agent_context["manager"].RLock()

                # Create a lock which can be used on Nordic
                # platforms (nRF5 and Zephyer): performing a
                # JLink download to a board while JLink RTT logging
                # is active on any other board will often stop
                # the RTT logging even though the sessions are
                # aimed at debuggers with entirely different
                # serial numbers.
                agent_context["misc_locks"]["jlink_lock"] = agent_context["manager"].RLock()

                # Create a "lock" that can be used on STM32F4
                # platforms to ensure that all downloads are
                # completed before logging commences.  We
                # can do this, rather than locking a tool for the
                # whole time as we have to do with Nordic, because
                # each STM32F4 board only runs a single instance
                agent_context["misc_locks"]["stm32f4_downloads_list"] = agent_context["manager"].list()

                # It is possible for some platforms to be a bit
                # pants at running in multiple instances
                # hence we provide a lock per platform that can
                # be passed into the instance for it to be able
                # to manage multiplicity if required.
                # However, we don't know what the platforms are
                # at this point so the list is empty
                agent_context["platform_locks"] = []

                # Create locks for the connections
                u_connection.init_locks(agent_context["manager"])

                # Launch a printer thread to print stuff out
                # nicely from multiple sources
                agent_context["print_queue"] = print_queue
                if agent_context["print_queue"] is None:
                    agent_context["print_queue"] = agent_context["manager"].Queue()
                agent_context["print_thread"] = u_utils.PrintThread(agent_context["print_queue"])
                agent_context["print_thread"].start()

                # Set up a printer for the agent to print to the queue
                agent_context["printer"] = u_utils.PrintToQueue(agent_context["print_queue"],
                                                                None, True)

                # Create an empty list of sessions
                agent_context["session_running_count"] = 0
                agent_context["next_session_id"] = 0
                agent_context["sessions"] = []

                # Done
                if u_settings.user_intervention_required():
                    # We only check this after doing all the setup so that
                    # the prints still get to the most useful destination
                    agent_context["printer"].string("{}cannot run, the user settings file(s) on"
                                                    " this agent need updating.".format(PROMPT))
                    stop()
                else:
                    agent_context["printer"].string("{}started.".format(PROMPT))
                    # Handle hw_reset
                    if hw_reset and USB_CUTTER_ID_STRS and (len(USB_CUTTER_ID_STRS) > 0):
                        agent_context["printer"].string("{}resetting USB...".format(PROMPT))
                        u_utils.usb_cutter_reset(USB_CUTTER_ID_STRS, agent_context["printer"], PROMPT)
                    success = True

    return success

def _stop(and_stay_dead=False):
    '''Stop the agent'''
    global AND_STAY_DEAD

    # Nothing in here, or called from here, must
    # rely on CONTEXT_LOCK or CONTEXT_MANAGER being
    # non-None

    agent_context = get()
    if agent_context:
        printer = agent_context["printer"]

        printer.string("{}stopping...".format(PROMPT))
        # Terminate processes in any running sessions
        printer.string("{}{} session(s) running.".format(PROMPT,
                       agent_context["session_running_count"]))
        for session in agent_context["sessions"]:
            session["running_flag"].clear()

        # Stop the printer thread
        printer.string("{}stopped.".format(PROMPT))
        sleep(1)
        agent_context["print_thread"].stop_thread()
        agent_context["print_thread"].join()

        # Clean up
        for session in agent_context["sessions"]:
            if "reporter" in session and session["reporter"]:
                session["reporter"].close()
                session["reporter"] = None
            if "report_thread" in session and session["report_thread"]:
                session["report_thread"].stop_thread()
                session["report_thread"].join()
                session["report_thread"] = None
            if "summary_report_handle" in session and session["summary_report_handle"]:
                session["summary_report_handle"].close()
                session["summary_report_handle"] = None
        agent_context.stop_thread()
        agent_context.join()
    if and_stay_dead:
        AND_STAY_DEAD = True

def stop(and_stay_dead=False):
    '''Wrapper for stop() to handle locking'''
    # It is possible for this to be called
    # asynchronously to recover from a situation
    # where all controllers have disconnected
    # without notice, in which case the context
    # lock will have been deliberately vapourised
    # to prevent deadlocks
    if CONTEXT_LOCK:
        with CONTEXT_LOCK:
            _stop(and_stay_dead)
    else:
        _stop(and_stay_dead)

def restart(hw_reset=False):
    '''Restart an agent'''
    success = False
    with CONTEXT_LOCK:
        print_queue = None
        agent_context = get()
        if agent_context and "print_queue" in agent_context:
            print_queue = agent_context["print_queue"]
        stop()
        success = start(print_queue, hw_reset=hw_reset)

    return success

def session_running_count():
    '''Get the number of sessions running on this agent'''
    with CONTEXT_LOCK:
        count = 0
        agent_context = get()
        if agent_context:
            count = agent_context["session_running_count"]

    return count

def session_running_names():
    '''Get the names (where present) of running sessions'''
    with CONTEXT_LOCK:
        names = []
        agent_context = get()
        if agent_context:
            for session in agent_context["sessions"]:
                name = session["name"]
                if name:
                    names.append(name)

    return names.copy()

def instance_running_count(session_name=None):
    '''Get the number of instances running on this agent'''
    with CONTEXT_LOCK:
        count = 0
        agent_context = get()
        if agent_context:
            if agent_context["session_running_count"] > 0:
                for session in agent_context["sessions"]:
                    if session_name:
                        if ("name" in session) and (session_name == session["name"]):
                            for process in session["processes"]:
                                if process["running_flag"].is_set():
                                    count += 1
                            break
                    else:
                        for process in session["processes"]:
                            if process["running_flag"].is_set():
                                count += 1
    return count

def instances_running(session_name=None):
    '''Get the instances running on this agent'''
    with CONTEXT_LOCK:
        instances = []
        agent_context = get()
        if agent_context:
            if agent_context["session_running_count"] > 0:
                for session in agent_context["sessions"]:
                    if session_name:
                        if ("name" in session) and (session_name == session["name"]):
                            for process in session["processes"]:
                                if process["running_flag"].is_set():
                                    instances.append(process["instance"].copy())
                            break
                    else:
                        for process in session["processes"]:
                            if process["running_flag"].is_set():
                                instances.append(process["instance"].copy())

    return instances

# Call this while session_run() is in progress
# and it will clear the "running" flag.  The
# sub-processes will then exit cleanly, in their
# own time.
def session_abort(session_name):
    '''Stop the given session (though don't wait for it to end)'''
    success = False
    with CONTEXT_LOCK:
        agent_context = get()
        if agent_context and (agent_context["session_running_count"] > 0):
            for session in agent_context["sessions"]:
                if ("name" in session) and (session_name == session["name"]):
                    # Clear the running flag
                    session["running_flag"].clear()
                    if session["reporter"]:
                        session["reporter"].event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                                  u_report.EVENT_INFORMATION,
                                                  "session {} aborting...".format(session_name))
                    success = True
    return success

# The function signature here follows the form of the old
# version, the one that was in u_run_branch.py, tacking
# process_pool on the end and then session_name,
# print_queue/prompt and abort_on_first_failure as optional
# parameters.
def session_run(database, instances, filter_string,
                ubxlib_dir, working_dir, clean,
                summary_report_file, test_report_file, debug_file,
                process_pool, session_name=None,
                print_queue=None, print_queue_prompt=None,
                abort_on_first_failure=False, unity_dir=None):
    '''Start a session running the given instances'''
    session = {}
    summary_report_file_path = None
    test_report_file_path = None
    debug_file_path = None
    return_value = 0
    local_agent = False
    agent_context = None

    with CONTEXT_LOCK:

        # Start the agent if not already running
        agent_context = get()
        if agent_context:
            if print_queue:
                agent_context["print_thread"].add_forward_queue(print_queue, print_queue_prompt)
        else:
            return_value = -1
            # HW reset is false when the agent is started implicitly:
            # it is up to the caller to call agent.start() explicitly
            # if it wants a HW reset
            if start(print_queue, hw_reset=False):
                return_value = 0
                agent_context = get()
                local_agent = True

        if agent_context:
            printer = agent_context["printer"]

            # Name the session and add it to the session list
            session["id"] = agent_context["next_session_id"]
            agent_context["next_session_id"] += 1
            session["name"] = "session " + str(session["id"])
            if session_name:
                session["name"] = session_name

            # Set a flag to indicate that the session is
            # running: processes can watch this and, if it is
            # cleared, they must exit at the next opportunity
            session["running_flag"] = CONTEXT_MANAGER.Event()
            session["running_flag"].set()
            session["process_running_count"] = 0
            session["processes"] = []
            agent_context["sessions"].append(session)
            agent_context["session_running_count"] += 1

            # Launch a thread that manages reporting
            # from multiple sources
            session["report_queue"] = None
            session["reporter"] = None
            session["report_thread"] = None
            session["summary_report_handle"] = None
            if summary_report_file:
                summary_report_file_path = working_dir + os.sep + summary_report_file
                session["summary_report_handle"] = open(summary_report_file_path, "w")
                if session["summary_report_handle"]:
                    printer.string("{}writing summary report to \"{}\".".  \
                                  format(PROMPT, summary_report_file_path))
                else:
                    printer.string("{}unable to open file \"{}\" for summary report.".   \
                                   format(PROMPT, summary_report_file_path))
                session["report_queue"] = agent_context["manager"].Queue()
                session["report_thread"] = u_report.ReportThread(session["report_queue"],
                                                                 session["summary_report_handle"])
                session["report_thread"].start()
                session["reporter"] = u_report.ReportToQueue(session["report_queue"], None, None,
                                                             agent_context["printer"])
                session["reporter"].open()

            # Add any new platform locks required for these instances
            create_platform_locks(database, instances,
                                  agent_context["manager"],
                                  agent_context["platform_locks"])

            # Set up all the instances
            for instance in instances:
                # Provide a working directory that is unique
                # for each instance and make sure it exists
                if working_dir:
                    this_working_dir = working_dir + os.sep +       \
                                       INSTANCE_DIR_PREFIX + \
                                       u_utils.get_instance_text(instance)
                else:
                    this_working_dir = os.getcwd() + os.sep +       \
                                       INSTANCE_DIR_PREFIX + \
                                       u_utils.get_instance_text(instance)

                if not os.path.isdir(this_working_dir):
                    os.makedirs(this_working_dir)
                # Only clean the working directory if requested
                if clean:
                    u_utils.deltree(this_working_dir, printer, PROMPT)
                    os.makedirs(this_working_dir)

                # Create the file paths for this instance
                if summary_report_file:
                    summary_report_file_path = this_working_dir + os.sep + summary_report_file
                if test_report_file:
                    test_report_file_path = this_working_dir + os.sep + test_report_file
                if debug_file:
                    debug_file_path = this_working_dir + os.sep + debug_file

                # Start u_run.main in each worker thread
                process = {}
                process["platform"] = u_data.get_platform_for_instance(database, instance)
                process["instance"] = instance
                # Create a flag to be set by u_run. while the process is running
                process["running_flag"] = CONTEXT_MANAGER.Event()
                process["platform_lock"] = None
                process["connection_lock"] = u_connection.get_lock(instance)
                for platform_lock in agent_context["platform_locks"]:
                    if process["platform"] == platform_lock["platform"]:
                        process["platform_lock"] = platform_lock["lock"]
                        break

                process["handle"] = process_pool.apply_async(u_run.main,
                                                             (database, instance,
                                                              filter_string, True,
                                                              ubxlib_dir,
                                                              this_working_dir,
                                                              process["connection_lock"],
                                                              process["platform_lock"],
                                                              agent_context["misc_locks"],
                                                              agent_context["print_queue"],
                                                              session["report_queue"],
                                                              summary_report_file_path,
                                                              test_report_file_path,
                                                              debug_file_path,
                                                              session["running_flag"],
                                                              process["running_flag"],
                                                              unity_dir))
                session["process_running_count"] += 1
                session["processes"].append(process)

    # The lock is released while we're running so that others can get in
    if agent_context:
        try:
            # IMPORTANT: need to be careful here with the bits of context
            # referenced while the context lock is released. Stick to things
            # within a session (or a process of a session) and don't remove
            # sessions.  That way it won't conflict with other calls into this
            # agent.

            # Wait for all the launched processes to complete
            printer.string("{}all instances now launched.".format(PROMPT))
            loop_count = 0
            while agent_context.is_alive() and (session["process_running_count"] > 0):
                for process in session["processes"]:
                    instance_text = u_utils.get_instance_text(process["instance"])
                    if not "stopped" in process and process["handle"].ready():
                        try:
                            # If the return value has gone negative, i.e.
                            # an infrastructure failure, leave it there,
                            # else add the number of test failures to it
                            if (return_value >= 0 and process["handle"].get() > 0) or \
                                (return_value <= 0 and process["handle"].get() < 0):
                                return_value += process["handle"].get()
                            if (return_value != 0) and abort_on_first_failure:
                                session["running_flag"].clear()
                                printer.string("{}an instance has failed, aborting"    \
                                               " (gracefully, might take a while)"     \
                                               " as requested...".                     \
                                               format(PROMPT))
                                abort_on_first_failure = False
                        except Exception as ex:
                            # If an instance threw an exception then flag an
                            # infrastructure error
                            traceback_str = traceback.format_exc()
                            return_value = -1
                            printer.string("{}instance {} threw exception \"{}: {}\"".
                                           format(PROMPT, instance_text,
                                                  type(ex).__name__, str(ex)))
                            printer.string(f"{PROMPT} {traceback_str}")
                            if session["reporter"]:
                                session["reporter"].event(u_report.EVENT_TYPE_INFRASTRUCTURE,
                                                          u_report.EVENT_FAILED,
                                                          "instance {} threw exception \"{}: {}\"". \
                                                          format(instance_text, type(ex).__name__,
                                                          str(ex)))
                        process["stopped"] = True
                        session["process_running_count"] -= 1
                        if session["process_running_count"] <= 0:
                            session["stopped"] = True
                    if not process["handle"].ready() and                         \
                       (loop_count == STILL_RUNNING_REPORT_SECONDS):
                        printer.string("{}instance {} still running.".           \
                                       format(PROMPT, instance_text))
                loop_count += 1
                if loop_count > STILL_RUNNING_REPORT_SECONDS:
                    loop_count = 0
                sleep(1)
        except KeyboardInterrupt:
            # Start things cleaning up
            session["running_flag"].clear()
            raise KeyboardInterrupt from ex

    #  Now need to lock again while we're manipulating stuff
    with CONTEXT_LOCK:

        if agent_context:
            # Remove the session from the list
            idx_to_remove = None
            for idx, item in enumerate(agent_context["sessions"]):
                if item["id"] == session["id"]:
                    idx_to_remove = idx
                    break
            if idx_to_remove is not None:
                agent_context["session_running_count"] -= 1
                agent_context["sessions"].pop(idx_to_remove)

            # Tidy up
            if session["reporter"]:
                session["reporter"].event_extra_information("return value overall {} (0 = success," \
                                                            " negative = probable infrastructure"   \
                                                            " failure, positive = failure(s) (may"  \
                                                            " still be due to infrastructure))".    \
                                                            format(return_value))
                session["reporter"].close()
            if session["report_thread"]:
                session["report_thread"].stop_thread()
                session["report_thread"].join()
                session["report_thread"] = None
            if session["summary_report_handle"]:
                session["summary_report_handle"].close()
                session["summary_report_handle"] = None

            printer.string("{}run(s) complete, return value {}.".
                           format(PROMPT, return_value))
            if local_agent:
                stop()
            else:
                if print_queue:
                    agent_context["print_thread"].remove_forward_queue(print_queue)

    return return_value

if __name__ == '__main__':
    freeze_support()
