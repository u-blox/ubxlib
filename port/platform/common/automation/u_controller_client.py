#!/usr/bin/env python

'''A version of u_controller.py which interacts with u_agent_service.py.'''

from time import time, sleep, gmtime, strftime
from multiprocessing.dummy import Pool as ThreadPool
import socket   # for socket.timeout
import sys      # for exit() and stdout
import queue
import argparse
import tempfile
import requests # For HTTP file uploads
import rpyc     # RPyC, mmmmm :-)
import u_data   # Gets the instance database
import u_select # Decide what to run for ourselves
import u_utils  # For commit_message_parse()

# Prefix to put at the start of all prints
PROMPT = "u_controller_client: "

# Ignore instance/filter, run the lot: a safety switch
RUN_EVERYTHING = False

# The default port to find agents on
AGENT_SERVICE_PORT = u_utils.AGENT_SERVICE_PORT

# The maxium number of characters from a controller name
# that will be used when constructing ubxlib_path
AGENT_UBXLIB_PATH_CONTROLLER_NAME_MAX_LENGTH = u_utils.AGENT_UBXLIB_PATH_CONTROLLER_NAME_MAX_LENGTH

# The number of times to retry an agent reconnection
# attempt at one second intervals
AGENT_RECONNECT_RETRIES = 30

# A list of platforms, taken from DATABASE.md, which
# cannot be [fully or partially] run in parallel and hence
# should be run on different agents if at all possible.
# For example ESP-IDF locks the system to do a tools
# update and so it is best not to run an ESP-IDF-based
# instance where one is already running, while nRF5SDK and
# Zephyr/nRF share the JLink tools which don't like being
# run in parallel even on apparently separate targets on the
# same machine.  The agent script will ensure that these
# are managed, this is just a helper to try to allocate
# instances to agents such that they will run most quickly.
NOT_SIMULTANEOUS_PLATFORMS = [["ESP-IDF"], ["STM32Cube"], ["nRF5SDK", "Zephyr"]]

# The guard timeout for the testing portion to complete
GUARD_TIMEOUT_SECONDS = 3600 * 2

# The amount of time to poll any agent for, handling stuff
# at RPyC level
AGENT_POLL_TIME = 2

# Intervals between reports of result status
RESULT_REPORT_INTERVAL = 30

# How long to expect starting up of an agent to take in minutes.
AGENT_STARTUP_DELAY_MINUTES = 1

# Place to hook a print queue
PRINT_QUEUE = None

# Place to hook a printer
PRINTER = None

# How long to buffer strings on the agent before sending them
# all to the controller (to speed things up)
BUFFER_TIME_SECONDS = 5

# How long to wait for any remaining debug to arrive
# when closing an agent down
WAIT_FOR_REMAINING_DEBUG_TIME = 0

# Connect to an agent.
def agent_connect(ip_address, port):
    '''Connect to the agent at the given IP address and port'''
    return rpyc.utils.factory.connect(ip_address, port, config={# Need this to retrieve lists
                                                                "allow_pickle": True,
                                                                "sync_request_timeout": None,
                                                                "allow_safe_attrs": True,
                                                                 # Need this to get queue output
                                                                 # from agent (.put())
                                                                 "allow_public_attrs": True})

# Close an agent, optionally printing something out.
def agent_close(agent, text=None):
    '''Close an agent'''
    if text and PRINTER:
        PRINTER.string("{}{}.".format(PROMPT, text))
    if agent["connection"]:
        if PRINTER:
            PRINTER.string("{}closing agent {}.".format(PROMPT, agent["name"]))
        agent["connection"].close()
        agent["connection"] = None

# Run a function on an agent, returning the outcome
# and closing the agent if the call fails.
def agent_call(agent, function, *args, **kwargs):
    '''Call a function on an agent'''
    result = None
    if agent["connection"]:
        try:
            result = getattr(agent["connection"].root, function)(*args, **kwargs)
        except (ConnectionRefusedError, EOFError) as ex:
            text = "error \"{} {}\" calling function {}.".format(type(ex).__name__, str(ex), function)
            agent_close(agent, text)
    return result

# Run a function on an agent asynchronously,
# returning an asynchrounous result object
# or None and closing the agent if the call fails.
def agent_call_async(agent, timeout, function, *args, **kwargs):
    '''Call a function on an agent asynchronously'''
    async_result_object = None
    if agent["connection"]:
        try:
            async_function = rpyc.async_(getattr(agent["connection"].root, function))
            async_result_object = async_function(*args, **kwargs)
            if timeout:
                async_result_object.set_expiry(timeout)
        except (ConnectionRefusedError, EOFError) as ex:
            text = "error {} {} calling function {} (asynchronously).". \
                   format(type(ex).__name__, str(ex), function)
            agent_close(agent, text)
    return async_result_object

# Poll an agent.
def agent_poll(agent, for_time=0):
    '''Poll an agent'''
    success = False
    if agent["connection"]:
        try:
            agent["connection"].poll_all(for_time)
            success = True
        except ConnectionRefusedError as ex:
            text = "error {} {} polling {}.".format(type(ex).__name__, str(ex), agent["name"])
            agent_close(agent, text)
    return success

# Connect to all available agents that can be found
# listening at the given port.
def agents_connect(port):
    '''Connect to all available agents and read their vitals'''
    agents = []
    hosts = []

    text = ""
    if port:
        text = " on port {}".format(port)
    if PRINTER:
        PRINTER.string("{}asking the RPyC registry for test agents{}...".format(PROMPT, text))
    try:
        hosts = rpyc.discover("Agent")
    except rpyc.utils.factory.DiscoveryError as ex:
        if PRINTER:
            PRINTER.string("{}{} - {} (is rpyc_registry.py running somewhere?).".  \
                           format(PROMPT, type(ex).__name__, str(ex)))
    if hosts:
        for host in hosts:
            if not port or (host[1] == port):
                agent = {}
                text = "found {}:{}".format(host[0], host[1])
                try:
                    # Set the important things for each agent
                    agent["connection"] = agent_connect(host[0], host[1])
                    agent["host_ip_address"] = host[0]
                    agent["port"] = host[1]
                    agent["name"] = agent_call(agent, "name_get")
                    if agent["name"]:
                        text += " \"{}\"".format(agent["name"])
                    else:
                        agent["name"] = agent["host_ip_address"]
                    agent["agent_update_branch_or_hash"] = None
                    agent["instances_allocated"] = []
                    agent["locked"] = False
                    agent["async_result_object"] = None
                    agent["agent_result_path"] = None
                    agent["agent_result"] = -1
                    # Get the activated instances for information
                    agent["instances_activated"] = u_utils.copy_two_level_list(agent_call(agent, "instances_activated_get"))
                    text += ", instance(s) activated"
                    instance_text = " none"
                    if agent["instances_activated"]:
                        instance_text = u_utils.get_instances_text(agent["instances_activated"])
                    text += instance_text
                    agents.append(agent)
                except (ConnectionRefusedError, socket.timeout):
                    text += ", unable to connect"
                if PRINTER:
                    PRINTER.string("{}{}.".format(PROMPT, text))
    return agents

# Update the given agent; the agent must have
# already been locked.  If the agent cannot be
# updated it is unlocked and the connection to
# it is closed.
def agent_update(agent_locked, url, controller_name):
    '''Update the given agent to the given hash or branch'''
    if agent_locked["agent_update_branch_or_hash"]:
        if PRINT_QUEUE:
            # Grab the printer output for this bit
            agent_call(agent_locked, "printer_start", PRINT_QUEUE, BUFFER_TIME_SECONDS)
        if PRINTER:
            PRINTER.string("{}stopping agent {} for update...".format(PROMPT,
                                                                      agent_locked["name"]))
        if agent_call(agent_locked, "stop", 0, controller_name) and   \
           agent_call(agent_locked, "agent_check_out", url,
                      agent_locked["agent_update_branch_or_hash"],
                      controller_name):
            # Must now restart the agent
            if PRINTER:
                PRINTER.string("{}restarting agent {}...".format(PROMPT, agent_locked["name"]))
                # Not checking return value here because it will be an
                # error due to the restart
                agent_call(agent_locked, "agent_restart", controller_name)
                # The connection to the agent will drop,
                # we must now reconnect to it
                agent_locked["connection"] = None
                retries = 0
                while not agent_locked["connection"] and (retries < AGENT_RECONNECT_RETRIES):
                    PRINTER.string("{}{}, reconnect attempt {}...".format(PROMPT,
                                                                          agent_locked["name"],
                                                                          retries + 1))
                    try:
                        agent_locked["connection"] = agent_connect(agent_locked["host_ip_address"],
                                                                   agent_locked["port"])
                    except ConnectionRefusedError:
                        pass
                    sleep(1)
                    retries += 1
                if agent_locked["connection"]:
                    PRINTER.string("{}reconnected to {}.".format(PROMPT, agent_locked["name"]))
                else:
                    PRINTER.string("{}unable to reconnect to agent {} "  \
                                   " BUT IT IS STILL LOCKED to {}!!!".format(PROMPT,
                                                                             agent_locked["name"],
                                                                             controller_name))
            else:
                agent_call(agent_locked, "unlock", controller_name)
                agent_locked["locked"] = False
                agent_close(agent_locked, "unable to restart agent")
        else:
            agent_call(agent_locked, "unlock", controller_name)
            agent_locked["locked"] = False
            agent_close(agent_locked, "unable to check code out onto agent")
        if PRINT_QUEUE:
            agent_call(agent_locked, "printer_stop", PRINT_QUEUE)
    return agent_locked["connection"] is not None

# Called by agents_lock_and_update(), separated so that it can be
# run in a thread for parallelism: lock and check to see if an
# agent needs an update. If an agent can't be locked it will be
# closed.
def _agent_lock_and_check(agent, controller_name, master_hash, branch_or_hash,
                          testing_master, automation_changes):
    '''Lock and check to see if an agent needs updating'''
    if PRINTER:
        PRINTER.string("{}checking {}...".format(PROMPT, agent["name"]))
    if agent_call(agent, "lock", controller_name):
        agent["locked"] = True
        if PRINTER:
            PRINTER.string("{}{} is locked to this controller.".format(PROMPT,
                                                                       agent["name"]))
        # Able to lock the agent, get its hash and branch now that
        # we're locked
        # IMPORTANT from now on DON'T FORGET to UNLOCK the agent
        # before closing it
        agent["agent_branch"] = agent_call(agent, "agent_branch_get")
        agent["agent_hash"] = agent_call(agent, "agent_hash_get")
        if agent["agent_branch"] and agent["agent_hash"]:
            if PRINTER:
                PRINTER.string("{}{} is on branch \"{}\" #{}". \
                               format(PROMPT, agent["name"],
                                      agent["agent_branch"],
                                      agent["agent_hash"]))
            # Determine if the agent would need to be updated
            if ((testing_master and (agent["agent_hash"] != master_hash)) or  \
                ((not testing_master) and automation_changes and
                 ((not branch_or_hash) or (agent['agent_hash'] not in branch_or_hash)))):
                # The agent would need to be updated
                agent["agent_update_branch_or_hash"] = branch_or_hash
                if PRINTER:
                    PRINTER.string("{}{} needs updating to {}.".format(PROMPT,
                                                                       agent["name"],
                                                                       branch_or_hash))
            else:
                if PRINTER:
                    PRINTER.string("{}{} needs no update: either we're testing master"   \
                                   " and it is on master or there have been no"          \
                                   " automation changes".format(PROMPT, agent["name"]))
            # If the agent is running something and would need to
            # be updated then just unlock it and forget about it
            if agent["agent_update_branch_or_hash"] and \
               agent_call(agent, "instance_running_count_get") > 0:
                agent_call(agent, "unlock", controller_name)
                agent["locked"] = False
                agent_close(agent, "agent would need update but is running something")
        else:
            agent_call(agent, "unlock", controller_name)
            agent["locked"] = False
            agent_close(agent, "unable to get agent branch and hash")
    else:
        # For debug purpose, print out who the agent is locked to
        text = agent_call(agent, "controller_get")
        if text:
            text = "unable to lock agent, it is locked to \"{}\"".format(text)
        else:
            text = "unable to lock agent"
        agent_close(agent, text)

# Called by agents_lock_and_update(), separated so that it can be
# run in a thread for parallelism: ensure that each locked agent is
# updated.  If an agent can't be updated it will be unlocked and
# closed.
def _agent_update(agent_locked, controller_name, url):
    '''Update an agent, if it needs it'''
    if agent_locked["connection"]:
        if (not agent_locked["agent_update_branch_or_hash"]) or  \
           agent_update(agent_locked, url, controller_name):
            agent_locked["instances_activated"] = u_utils.copy_two_level_list(agent_call(agent_locked,
                                                                                         "instances_activated_get"))
            agent_locked["instances_running"] = u_utils.copy_two_level_list(agent_call(agent_locked,
                                                                                       "instances_running_get"))
        else:
            agent_call(agent_locked, "unlock", controller_name)
            agent_locked["locked"] = False
            agent_close(agent_locked, "unable to update agent")

# Lock and update suitable agents, closing the rest.
# An agent must be updated if (a) we're testing master
# and the agent does not have the same hash or (b) we've
# been given a branch name and there have been automation
# changes or (c) we've been given a hash, the agent
# isn't at that hash and there have been automation changes.
# Any failure will result in the agent being dropped from
# the list with connection closed and unlocked.
# Returns a list of locked agents.
def agents_lock_and_update(agents, controller_name, url,
                           master_hash, branch_or_hash,
                           files_changed):
    '''Lock and, if necessary, update the given agents'''
    the_hash = None
    testing_master = False
    agents_locked = []

    if branch_or_hash.startswith("#"):
        the_hash = branch_or_hash[1:len(branch_or_hash)]
    if master_hash.startswith("#"):
        master_hash = master_hash[1:len(master_hash)]
    if (the_hash and (the_hash == master_hash)) or \
        ((not the_hash) and (branch_or_hash == "master")):
        testing_master = True

    # Use the file list to determine if there have been any
    # changes in automation between master and branch_or_hash
    automation_changes = u_select.automation_changes(files_changed)

    if PRINTER:
        PRINTER.string("{}### locking and updating agents...".format(PROMPT))
        text = "we're"
        if not testing_master:
            text += " not"
        text += " testing master"
        if automation_changes:
            text += " and there have been automation changes"
        PRINTER.string("{}{}.".format(PROMPT, text))

    # Lock and check if each agent needs updating in a thread pool for
    # greater speed.
    pool = ThreadPool(len(agents))
    for agent in agents:
        pool.apply_async(_agent_lock_and_check, (agent, controller_name, master_hash, branch_or_hash,
                         testing_master, automation_changes))
    pool.close()
    pool.join()

    # Now any agents still open will have been locked; update the ones
    # that need it and get the instances_activated and
    # instances_running counts now that they're locked, again in
    # a thread pool for speed
    pool = ThreadPool(len(agents))
    for agent in agents:
        if agent["connection"]:
            pool.apply_async(_agent_update, (agent, controller_name, url))
    pool.close()
    pool.join()

    # Now make a list of the remaining agents, all of which will be locked
    for agent in agents:
        if agent["connection"]:
            agents_locked.append(agent)

    if PRINTER:
        PRINTER.string("{}found {} agent(s) we could run on.".format(PROMPT, len(agents_locked)))

    return agents_locked

# Update the estimated run duration for an agent.
def agent_update_running_duration(agent, database):
    '''Update run duration for an agent'''
    largest_duration = 0

    agent.pop("instances_largest_duration", None)
    agent["instances_running_duration"] = 0
    if len(agent["instances_running"]) > 0:
        for instance in agent["instances_running"]:
            duration = u_data.get_duration_for_instance(database, instance)
            if duration:
                agent["instances_running_duration"] += duration
            if duration > largest_duration:
                largest_duration = duration
    agent["instances_largest_duration"] = largest_duration

# Sort agents so that the least busy is at the top.
def agents_busy_sort(agents, database):
    '''Sort a list of agents, least busy first'''

    # Work out the total expected duration
    # of the instance runs per agent
    for agent in agents:
        agent_update_running_duration(agent, database)
    # Now sort the agent list according to the
    # duration
    agents.sort(key=lambda item: item["instances_running_duration"])

# Sort agents so that the one we have allocated most to is at the top.
def agents_allocated_sort(agents, database):
    '''Sort a list of agents, the one we have allocated most to at the top'''

    # Work out the total expected duration
    # of the instances allocated per agent
    for agent in agents:
        agent["instances_allocated_duration"] = 0
        if len(agent["instances_allocated"]) > 0:
            for instance in agent["instances_allocated"]:
                duration = u_data.get_duration_for_instance(database, instance)
                if duration:
                    agent["instances_allocated_duration"] += duration
    # Now sort the agent list according to the
    # duration
    agents.sort(key=lambda item: item["instances_allocated_duration"], reverse = True)

# Allocate instances to agents.  The aim is
# to spread runs across agents so that no single
# agent is unduly loaded, taking into account
# NOT_SIMULTANEOUS_PLATFORMS so that things that can't
# be run in parallel don't end up on the same agent.
# Returns the list of agents on which instances have been
# allocated.  Any agents to which nothing has been
# allocated are unlocked and closed.
def instances_allocate(agents_locked, database, instances, controller_name):
    '''Allocate instances to locked agents'''
    abort_run = False
    allocated = 0
    agents_locked_allocated = []

    if PRINTER:
        PRINTER.string("{}### allocating instance(s) to agents...".format(PROMPT))
    # First run around this loop looking for optimal homes
    # for each instance
    for idx, instance_to_distribute in enumerate(instances):
        if instance_to_distribute:
            # If we've not yet distributed this instance...
            instance_to_distribute_platform = u_data.get_platform_for_instance(database,
                                                                               instance_to_distribute)
            instance_to_distribute_platform_text = instance_to_distribute_platform
            if not instance_to_distribute_platform_text:
                instance_to_distribute_platform_text = "no platform"
            good = False
            # Sort the agent list so that the one running
            # the fewest things is at the top
            agents_busy_sort(agents_locked, database)
            for agent in agents_locked:
                # Check each agent
                if instance_to_distribute in agent["instances_activated"]:
                    # This one can run the instance
                    text = "agent {} can run instance {} ({})".   \
                           format(agent["name"],
                                  u_utils.get_instance_text(instance_to_distribute),
                                  instance_to_distribute_platform_text)
                    good = True
                    if agent["instances_running"]:
                        # But there are already some instances running on the agent...
                        for instance_running in agent["instances_running"]:
                            instance_running_platform = u_data.get_platform_for_instance(database,
                                                                                         instance_running)
                            instance_running_platform_text = instance_running_platform
                            if not instance_running_platform_text:
                                instance_running_platform_text = "no platform"
                            # Check each item in the NOT_SIMULTANEOUS_PLATFORMS list...
                            for not_simultaneous_platform in NOT_SIMULTANEOUS_PLATFORMS:
                                # If the platform of the instance to distribute is
                                # in the NOT_SIMULTANEOUS_PLATFORMS entry and the
                                # platform of an instance that is running is also
                                # in it then don't allocate it here
                                if (instance_to_distribute_platform in not_simultaneous_platform) and  \
                                   (instance_running_platform in not_simultaneous_platform):
                                    text += " but is already running instance {} ({})"       \
                                            " and those platforms can only be run in series".\
                                            format(u_utils.get_instance_text(instance_running),
                                                   instance_running_platform_text)
                                    good = False
                                    break
                    if PRINTER:
                        PRINTER.string("{}{}.".format(PROMPT, text))
                    if good:
                        agent["instances_allocated"].append(instance_to_distribute.copy())
                        # Add it to the running list even though it is not yet running
                        # in order that the check above works
                        agent["instances_running"].append(instance_to_distribute.copy())
                        instances[idx] = None
                        allocated += 1
                        break
                else:
                    # This one cannot run the instance
                    if PRINTER:
                        PRINTER.string("{}agent {} cannot run instance {}.".   \
                                       format(PROMPT, agent["name"],
                                              u_utils.get_instance_text(instance_to_distribute)))

    text = "{} instance(s) allocated optimally".format(allocated)
    if allocated > 0:
        if PRINTER:
            PRINTER.string("{}{}:".format(PROMPT, text))
        for agent in agents_locked:
            text = " nothing"
            if agent["instances_allocated"]:
                text = u_utils.get_instances_text(agent["instances_allocated"])
            if PRINTER:
                PRINTER.string("{}- {} will run{}.".format(PROMPT, agent["name"], text))
    else:
        if PRINTER:
            PRINTER.string("{}{}.".format(PROMPT, text))
    if len(instances) - allocated > 0:
        if PRINTER:
            PRINTER.string("{}left with instance(s){}.".format(PROMPT,
                                                               u_utils.get_instances_text(instances)))

    while (not abort_run) and (allocated < len(instances)):
        for idx, instance_to_distribute in enumerate(instances):
            if instance_to_distribute:
                # Sort the agent list so that the one running
                # the fewest things is at the top
                agents_busy_sort(agents_locked, database)
                for agent in agents_locked:
                    if instance_to_distribute in agent["instances_activated"]:
                        agents_locked[0]["instances_allocated"].append(instance_to_distribute.copy())
                        agents_locked[0]["instances_running"].append(instance_to_distribute.copy())
                        if PRINTER:
                            PRINTER.string("{}allocated instance {} to agent {}.". \
                                           format(PROMPT,
                                                  u_utils.get_instance_text(instance_to_distribute),
                                                  agents_locked[0]["name"]))
                        instance_to_distribute = None
                        instances[idx] = None
                        allocated += 1
                        break
            if instance_to_distribute:
                abort_run = True
                if PRINTER:
                    PRINTER.string("{}cannot find an agent that can run instance {}, aborting!!!".
                                   format(PROMPT,
                                          u_utils.get_instance_text(instance_to_distribute)))
    # Either we have now allocated each instance to an
    # agent or we've failed and will just unlock
    # and close them
    for agent in agents_locked:
        if not abort_run:
            if agent["instances_allocated"]:
                agent_update_running_duration(agent, database)
                agents_locked_allocated.append(agent)
            else:
                agent_call(agent, "unlock", controller_name)
                agent["locked"] = False
                agent_close(agent, "not using agent {}, it is now unlocked".format(agent["name"]))
        else:
            agent_call(agent, "unlock", controller_name)
            agent["locked"] = False
            agent_close(agent, " agent {} is unlocked".format(agent["name"]))
    if agents_locked_allocated:
        text = "now all {} instance(s) have been allocated".format(len(instances))
        if allocated > 0:
            if PRINTER:
                PRINTER.string("{}{}:".format(PROMPT, text))
            for agent in agents_locked:
                text = "{} will run".format(agent["name"])
                if agent["instances_allocated"]:
                    text += "{}".format(u_utils.get_instances_text(agent["instances_allocated"]))
                    instances_already_running = []
                    for instance_running in agent["instances_running"]:
                        if instance_running not in agent["instances_allocated"]:
                            instances_already_running.append(instance_running)
                    if instances_already_running:
                        text += ", and is already running {}". \
                                format(u_utils.get_instances_text(instances_already_running))
                    if "instances_largest_duration" in agent:
                        text += ", rough duration {} minute(s)". \
                                format(agent["instances_largest_duration"] + AGENT_STARTUP_DELAY_MINUTES)
                else:
                    text += " nothing"
                if PRINTER:
                    PRINTER.string("{}- {}.".format(PROMPT, text))
        else:
            if PRINTER:
                PRINTER.string("{}{}.".format(PROMPT, text))

    return agents_locked_allocated

# Start the instances running on the locked agents that have things
# allocated to them.
# Returns the list of agents on which things are running.  If an
# agent cannot be used all are closed and unlocked.
def instances_start(agents_locked_allocated, database, controller_name, url,
                    branch_or_hash, filter_string, summary_report_file,
                    test_report_file, debug_file, abort_on_first_failure,
                    timeout):
    '''Run instances on the locked agents'''
    agents_running = []
    abort_run = False

    if PRINTER:
        PRINTER.string("{}### starting instance(s)...".format(PROMPT))
    # Start the one we have allocated most to first to get them moving
    agents_allocated_sort(agents_locked_allocated, database)
    for agent in agents_locked_allocated:
        if not abort_run:
            if PRINT_QUEUE:
                # Grab the printer output from this agent from now on
                agent_call(agent, "printer_start", PRINT_QUEUE, BUFFER_TIME_SECONDS)
            # Check out the code we're going to run onto the agent
            if PRINTER:
                PRINTER.string("{}checking out {} {} on {}...".
                               format(PROMPT, url, branch_or_hash, agent["name"]))
            if agent_call(agent, "check_out", url, branch_or_hash, controller_name):
                if PRINTER:
                    PRINTER.string("{}running instance(s){} on agent {} with"
                                   " timeout {} second(s)...".
                                   format(PROMPT,
                                          u_utils.get_instances_text(agent["instances_allocated"]),
                                          agent["name"], timeout))
                # Perform the asynchronous run
                agent["session_name"] = controller_name + "_" + branch_or_hash
                agent["async_result_object"] = agent_call_async(agent, timeout,
                                                                "session_run",
                                                                database,
                                                                agent["instances_allocated"],
                                                                filter_string,
                                                                url, branch_or_hash,
                                                                summary_report_file,
                                                                test_report_file,
                                                                debug_file,
                                                                agent["session_name"],
                                                                controller_name,
                                                                abort_on_first_failure)
                if agent["async_result_object"]:
                    agent["start_time"] = gmtime()
                    # Get the path at which the results can be found on the agent
                    agent["agent_result_path"] = agent_call(agent, "result_path_get",
                                                            url, branch_or_hash,
                                                            controller_name)
                    if agent["agent_result_path"]:
                        count = len(agent["instances_allocated"])
                        agent["instances_running"] = u_utils.copy_two_level_list(agent_call(agent, "instances_running_get",
                                                                                            agent["session_name"]))
                        if agent["instances_running"]:
                            count += len(agent["instances_running"])
                        if PRINTER:
                            PRINTER.string("{}{} instance(s) running on agent {}.". \
                                           format(PROMPT, count, agent["name"]))
                        agents_running.append(agent)
                    else:
                        if PRINT_QUEUE:
                            agent_call(agent, "printer_stop", PRINT_QUEUE)
                        agent_call(agent, "unlock", controller_name)
                        agent["locked"] = False
                        agent_close(agent, "unable to get result path from agent")
                        abort_run = True
                else:
                    abort_run = True
                    if PRINT_QUEUE:
                        agent_call(agent, "printer_stop", PRINT_QUEUE)
                    agent_call(agent, "unlock", controller_name)
                    agent["locked"] = False
                    agent_close(agent, "unable to start instance run on agent")
                    abort_run = True
            else:
                if PRINT_QUEUE:
                    agent_call(agent, "printer_stop", PRINT_QUEUE)
                agent_call(agent, "unlock", controller_name)
                agent["locked"] = False
                agent_close(agent, "unable to check-out code on agent")
                abort_run = True

    if abort_run:
        PRINTER.string("{}unable to use one or more agents, aborting...".format(PROMPT))
        for agent in agents_locked_allocated:
            if PRINT_QUEUE:
                agent_call(agent, "printer_stop", PRINT_QUEUE)
            agent_call(agent, "unlock", controller_name)
            agent["locked"] = False
            agent_close(agent)
        agents_locked_allocated = []
        agents_running = []

    return agents_running

# Wait for instances to finish running, closing agents
# as they do so.
# Returns an error value: zero for success, negative for
# infrastructure failure, positive for tests failed.
def instances_wait(agents_running, controller_name, archive_url, archive_credentials,
                   instance_results_files, recurse, summary_results_file,
                   abort_on_first_failure):
    '''Wait for instances to finish running'''
    return_value = 0
    start_time = time()
    last_report_time = start_time
    agents_running_count = len(agents_running)
    failed_agent_name = None
    summary_file_handle = None

    if PRINTER:
        PRINTER.string("{}### waiting for instance(s) to finish...".format(PROMPT))
    if archive_url and summary_results_file:
        try:
            summary_file_handle = tempfile.TemporaryFile()
        except OSError:
            if PRINTER:
                PRINTER.string("{}unable to open a temporary file for the summary results.". \
                               format(PROMPT))
    while (agents_running_count > 0) and not failed_agent_name:
        for agent in agents_running:
            if agent["connection"]:
                text_result = ""
                agent_poll(agent, AGENT_POLL_TIME)
                if agent["async_result_object"].ready:
                    try:
                        agent["agent_result"] = agent["async_result_object"].value
                        if agent["agent_result"] != 0:
                            if abort_on_first_failure:
                                failed_agent_name = agent["name"]
                        text_result += ", gave return value {}".format(agent["agent_result"])
                    except Exception as ex:
                        if abort_on_first_failure:
                            failed_agent_name = agent["name"]
                        text_result += ", threw exception {} - {}".format(type(ex).__name__, str(ex))
                else:
                    if agent["async_result_object"].expired:
                        if abort_on_first_failure:
                            failed_agent_name = agent["name"]
                        text_result += " timed out"
                if text_result:
                    text = "{}, running{}".format(agent["name"],
                                                  u_utils.get_instances_text(agent["instances_allocated"]))
                    text += text_result
                    text += " after {}, results in [{}] {}".format(strftime("%H:%M:%S", gmtime(time() - start_time)),
                                                                   agent["host_ip_address"],
                                                                   agent["agent_result_path"].replace("\\", "/"))
                    # Close the printer and wait for any remaining
                    # debug prints to arrive
                    if PRINT_QUEUE:
                        agent_call(agent, "printer_stop", PRINT_QUEUE)
                    agent_poll(agent, WAIT_FOR_REMAINING_DEBUG_TIME)

                    # If we have an archive URL, ask the agent to put the
                    # instance result files there and then create the summary
                    # file and put it there
                    if archive_url:
                        if instance_results_files:
                            if agent_call(agent, "archive_to_url",
                                          agent["agent_result_path"], instance_results_files,
                                          archive_url, recurse, archive_credentials):
                                text += " and archived to {}".format(archive_url)
                            else:
                                text += " but COULD NOT archive files to {}".format(archive_url)
                        if summary_file_handle:
                            summary_file_handle.write(bytes(agent["name"] + "\n", encoding="utf8"))
                            if not agent_call(agent, "copy",
                                              summary_file_handle,
                                              agent["agent_result_path"],
                                             [summary_results_file]):
                                text += ", unable to copy {}".format(summary_results_file)
                    agent_close(agent, text)
                    agents_running_count -= 1
                    break
        if (time() > last_report_time + RESULT_REPORT_INTERVAL) or (agents_running_count == 0):
            last_report_time = time()
            text = ""
            for agent in agents_running:
                if agent["connection"]:
                    if text:
                        text += " "
                    text += "{}".format(agent["name"])
                    agent["instances_running"] = u_utils.copy_two_level_list(agent_call(agent, "instances_running_get",
                                                                                        agent["session_name"]))
                    if agent["instances_running"]:
                        if agent["locked"] and agent_call(agent, "unlock", controller_name):
                            if PRINTER:
                                PRINTER.string("{}agent {} now unlocked, lock is not required once instances are running.". \
                                               format(PROMPT, agent["name"]))
                            agent["locked"] = False
                        text += " [{}]".\
                                format(u_utils.get_instances_text(agent["instances_running"]).strip())
            if PRINTER:
                PRINTER.string("{}{} agent(s) ({}) still running after {}.". \
                               format(PROMPT, agents_running_count, text.strip(),
                                     strftime("%H:%M:%S", gmtime(time() - start_time))))
    if failed_agent_name:
        if PRINTER:
            PRINTER.string("{}aborting at first failure (on {}) as requested...". \
                           format(PROMPT, failed_agent_name))
        for agent in agents_running:
            agent_call(agent, "session_abort", agent["session_name"], controller_name)
            if agent["locked"] and agent_call(agent, "unlock", controller_name):
                if PRINTER:
                    PRINTER.string("{}agent {} now unlocked.". \
                                   format(PROMPT, agent["name"]))
                agent["locked"] = False
            if PRINT_QUEUE:
                agent_call(agent, "printer_stop", PRINT_QUEUE)
            agent_close(agent, "aborted")
            agents_running_count -= 1

    # Calculate the overall return value.
    # If any return value has gone negative, i.e.
    # an infrastructure failure, then add those
    # together and ignore positive values.  If there
    # are no negative values then add the positive
    # values together instead.
    negative = False
    for agent in agents_running:
        if agent["agent_result"] < 0:
            negative = True
            break
    for agent in agents_running:
        value = agent["agent_result"]
        if negative and (value < 0):
            return_value += value
        else:
            if (not negative) and (value > 0):
                return_value += value

    # Copy the collated summary results file over to the archive server
    if summary_file_handle:
        summary_file_handle.seek(0)
        destination = archive_url + "/" + summary_results_file
        if PRINTER:
            PRINTER.string("{}PUTing {} to {}...".format(PROMPT,
                                                         summary_results_file,
                                                         archive_url))
        try:
            if archive_credentials:
                response = requests.put(destination, files={controller_name: summary_file_handle},
                                        headers={"Content-Type": "text/plain", "Content-Disposition": "inline"},
                                        auth=tuple(archive_credentials.split(":")))
            else:
                response = requests.put(destination, files={controller_name: summary_file_handle},
                                        headers={"Content-Type": "text/plain", "Content-Disposition": "inline"})
            if PRINTER:
                PRINTER.string("{}...returned result {}.".format(PROMPT, response.status_code))
        except (ConnectionError, TimeoutError):
            if PRINTER:
                PRINTER.string("{}...failed because of a connection error.".format(PROMPT))
        summary_file_handle.close()

    if PRINTER:
        PRINTER.string("{}### done.".format(PROMPT))
        PRINTER.string("{}all agent(s) finished after {} with result {}.". \
                       format(PROMPT, strftime("%H:%M:%S", gmtime(time() - start_time)),
                              return_value))

    return return_value

if __name__ == "__main__":
    RETURN_VALUE = -1
    DATABASE = []
    INSTANCES = []
    FILTER_STRING = None

    # Switch off traceback to stop the horrid developmenty prints
    #sys.tracebacklimit = 0
    PARSER = argparse.ArgumentParser(description="A script to"      \
                                     " run examples/tests on"       \
                                     " ubxlib agent services"       \
                                     " connected over RPyC.")
    PARSER.add_argument("-s", help="a summary report should be"      \
                        " written to the given file, e.g."           \
                        " -s summary.txt; any existing file will be" \
                        " over-written.")
    PARSER.add_argument("-t", help="an XML test report should be"   \
                        " written to the given file, e.g."          \
                        " -t report.xml; any existing file will be" \
                        " over-written.")
    PARSER.add_argument("-d", help="debug output for each test"     \
                        " instance executed should be written to"   \
                        " the given file, e.g. -d debug.txt; any"   \
                        " existing file will be over-written.")
    PARSER.add_argument("-a", help="archive the -s, -t and -d files" \
                        " to this URL e.g. on a Nexus server,"       \
                        " something like -a"                         \
                        " http://nexus.blah.com:7000/thingy; if"     \
                        " credentials are required they must be"     \
                        " added with the -c option.")
    PARSER.add_argument("-c", help="the credentials for -a, if"      \
                        " required, e.g. if username and password"   \
                        " are required for a Nexus server something" \
                        " like myusername:mypassword.")
    PARSER.add_argument("-f", action="store_true", help="abort"     \
                        " on first failure.")
    PARSER.add_argument("-p", help="the port number to look for agents" \
                        " on, usually " + str(AGENT_SERVICE_PORT) + ".")
    PARSER.add_argument("-g", default=GUARD_TIMEOUT_SECONDS, help=""\
                        " guard timer when running tests in seconds,"\
                        " default " + str(GUARD_TIMEOUT_SECONDS) + ".")
    PARSER.add_argument("controller_name", help="a name for this"  \
                        " controller; please keep it to " +        \
                        str(AGENT_UBXLIB_PATH_CONTROLLER_NAME_MAX_LENGTH) + \
                        " characters or less as it will be used"   \
                        " in a path and the agent will shorten"    \
                        " it if it is longer.")
    PARSER.add_argument("url", help="the url of the ubxlib repo to" \
                        " test, e.g. https://github.com/u-blox/ubxlib.")
    PARSER.add_argument("master_hash", help="the hash of master.")
    PARSER.add_argument("branch_or_hash", help="the name or hash"   \
                        " of the branch to test; if using a hash"   \
                        " prefix it with '#' else head revision"   \
                        " of the given branch will be assumed."     \
                        " To avoid unnecessary agent updates"       \
                        " (since this code can't know what the"     \
                        " head revision of a given branch is) it"   \
                        " is better to specify a hash.")
    PARSER.add_argument("message", help="the text from the most"    \
                        " recent commit on branch_or_hash; please"  \
                        " replace any occurrence of \" with, say,"  \
                        " ` and linefeeds with \"\\n\" so that"     \
                        " the text survives being passed as a"      \
                        " parameter.")
    PARSER.add_argument("file", nargs='*', help="the file path(s)"  \
                        " changed between master_hash and"          \
                        " branch_or_hash.")
    ARGS = PARSER.parse_args()

    # We go multi-threaded, so set up a printer to handle the output
    PRINT_QUEUE = queue.Queue()
    PRINT_THREAD =  u_utils.PrintThread(PRINT_QUEUE)
    PRINT_THREAD.start()
    PRINTER = u_utils.PrintToQueue(PRINT_QUEUE, None, True)

    AGENTS = []
    try:
        # Get the instance DATABASE by parsing the data file
        DATABASE = u_data.get(u_data.DATA_FILE)
        if RUN_EVERYTHING:
            # Safety switch has been thrown, run the lot
            if PRINTER:
                PRINTER.string("{}settings \"RUN_EVERYTHING\" is True.".format(PROMPT))
            INSTANCES = u_data.get_instances_all(DATABASE)
        else:
            # Parse the message
            FOUND, FILTER_STRING = u_utils.commit_message_parse(ARGS.message, INSTANCES, PRINTER, PROMPT)
            if FOUND:
                if INSTANCES:
                    # Deal with the "run everything" case
                    if INSTANCES[0][0] == "*":
                        TEXT = "running everything"
                        if FILTER_STRING:
                            TEXT += " on API \"{}\"".format(FILTER_STRING)
                        TEXT += " at user request"
                        if PRINTER:
                            PRINTER.string("{}{}.".format(PROMPT, TEXT))
                        INSTANCES = u_data.get_instances_all(DATABASE)
            else:
                # No instance specified by the user, decide what to run
                FILTER_STRING = u_select.select(DATABASE, INSTANCES, ARGS.file)

            if INSTANCES:
                # Connect to all agents listening on the given port
                AGENTS = agents_connect(ARGS.p)
                if AGENTS:
                    # Lock the agents we can use and update them if required
                    AGENTS = agents_lock_and_update(AGENTS, ARGS.controller_name, ARGS.url,
                                                    ARGS.master_hash, ARGS.branch_or_hash,
                                                    ARGS.file)
                    if AGENTS:
                        # Decide where to run each instance
                        AGENTS = instances_allocate(AGENTS, DATABASE, INSTANCES,
                                                    ARGS.controller_name)
                        if AGENTS:
                            # Start the instances running on the agents
                            AGENTS = instances_start(AGENTS, DATABASE, ARGS.controller_name,
                                                     ARGS.url, ARGS.branch_or_hash, FILTER_STRING,
                                                     ARGS.s, ARGS.t, ARGS.d, ARGS.f, ARGS.g)
                            if AGENTS:
                                # Wait for the runs to complete and archive the results
                                INSTANCE_RESULTS_FILES = []
                                if ARGS.a:
                                    if ARGS.t:
                                        INSTANCE_RESULTS_FILES.append(ARGS.t)
                                    if ARGS.d:
                                        INSTANCE_RESULTS_FILES.append(ARGS.d)
                                RETURN_VALUE = instances_wait(AGENTS, ARGS.controller_name,
                                                              ARGS.a, ARGS.c, INSTANCE_RESULTS_FILES,
                                                              1, ARGS.s, ARGS.f)
                            else:
                                if PRINTER:
                                    PRINTER.string("{}unable to run any instances on any agents.". \
                                                   format(PROMPT))
                        else:
                            if PRINTER:
                                PRINTER.string("{}unable to allocate all instances to an agent.". \
                                               format(PROMPT))
                    else:
                        if PRINTER:
                            PRINTER.string("{}unable to lock any agents.".format(PROMPT))
                else:
                    if PRINTER:
                        PRINTER.string("{}no agents available.".format(PROMPT))
            else:
                if PRINTER:
                    PRINTER.string("{}*** WARNING: no instances to run! ***".format(PROMPT))
                RETURN_VALUE = 0
    except KeyboardInterrupt:
        if PRINTER:
            PRINTER.string("{}caught CTRL-C, stopping gracefully (might take"    \
                           " a while)...".format(PROMPT))
        # Send abort signals to all the agents that are running so
        # that they can tidy up in their own time
        for AGENT in AGENTS:
            if AGENT["async_result_object"]:
                agent_call(AGENT, "session_abort", AGENT["session_name"], ARGS.controller_name)
            agent_call(AGENT, "unlock", ARGS.controller_name)
            AGENT["locked"] = False
            agent_close(AGENT)

    if PRINTER:
        PRINTER.string("{}return value {} (0 = success, negative = probable" \
                       " infrastructure failure, positive = failure(s) (may" \
                       " still be due to infrastructure)).".format(PROMPT, RETURN_VALUE))

    # Stop the printer
    sleep(1)
    PRINT_THREAD.stop_thread()
    PRINT_THREAD.join()
    PRINTER = None

    sys.exit(RETURN_VALUE)
