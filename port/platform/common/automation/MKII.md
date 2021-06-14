# Introduction
This describes the MK II wrapper of the automated test system.  The MK I automated test system remains usable as-is and all of what is described in [README.md](README.md) still applies, the MK II wrapper simply replaces `u_run_branch.py` with a `u_controller_client.py`/`u_agent_service.py` distributed architecture, communicating over [RPyC](https://rpyc.readthedocs.io/), allowing the test load to be spread across multiple test agents, potentially shared between multiple Jenkins masters.

# Architecture
A configuration of the MK II automated test system might look something like this:

```
                          +------------------------+               +------------------------+
                          | u_controller_client.py |               | u_controller_client.py |
                          |          "c1"          |               |          "c2"          |
                          +------------------------+               +------------------------+

    +-------------------------------+    +-----------------------------------+    +-----------------------------------+
    |       u_agent_service.py      |    |         u_agent_service.py        |    |         u_agent_service.py        |
    |     "agent 1" (port 17003)    |    |       "agent 2" (port 17003)      |    |       "agent 3" (port 17003)      |            +------------------------+
    |           u_agent.py          |    |             u_agent.py            |    |             u_agent.py            |            |    rpyc_registry.py    |
    |                               |    |                                   |    |                                   |            |       (port 18811)     |
    |   u_run.py       u_run.py ... |    |    u_run.py          u_run.py ... |    |    u_run.py          u_run.py ... |            +------------------------+
    | u_run_lint.py u_run_zephyr.py |    | u_run_nrf5sdk.py u_run_esp_idf.py |    | u_run_zephyr.py  u_run_stm32f4.py |
    +-------------------------------+    +-----------------------------------+    +-----------------------------------+
```

- there may be one or more `u_controller_client.py`, each likely executed by the `Jenkinsfile` of a Jenkins server (in the same way as `u_run_branch.py` is in the MK I automated test system); each controller has a unique name, in the example above "c1" and "c2" - it is important to keep the controller name short as it is used as part of a path on the agent and we don't want path lengths becoming long,
- there may be one or more `u_agent_service.py`, each on a different PC, listening on a pre-agreed port number; `u_agent_service.py` exposes an RPyC interface to the world and runs the underlying `u_agent.py` which has a pool of logical processes in each of which an instance of `u_run.py` can be executed; `u_run.py` in turn calls the appropriate `u_run_xxx.py` script for the test instances (from `DATABASE.md`) supported by that agent and required by the controller on a given test run,
- somewhere on the same network `rpyc_registry.py` must be running on a\[nother\] pre-agreed port number (the RPyC default is 18811); this is a standard component of RPyC that provides a way for an agent to register its existence and a controller to find an agent; if using Jenkins then it may be sensible to run this on the Jenkins master.

Note: for `rpyc_registry.py` "the same network" means that there must be no router between it and the agents/controllers: the agents send packets to 255.255.255.255:18811 to find `rpyc_registry.py` and such "broadcast" packets do not cross a routing boundary.

Each `u_agent_service.py` can be configured to listen on any \[single\] port and each `u_controller_client.py` can either look for test agents on a \[single\] port or not filter by port number at all.  In this way the agents can either be shared (in the example above both "c1" and "c2" either look for agents on port 17003 or don't filter on port number) or they can be distinct (for example "agent 3" could be listening on port 17004 instead of 17003 and controller "c2" could be set up to look for agents on port 17004 only, leaving the agents listening on port 17003 for controller "c1" which would be set up to look for agents on port 17003 only).  When agents are shared between controllers there are some limitations to how they can be used so having a "pool" of agents per controller that listen on the same port number, distinct from the port number used for any other controller, is preferable.  A single `rpyc_registry.py` instance can always be shared in all cases since it doesn't care what port number the agents are listening on.

# Deciding What Instances To Support On Each Test Agent
All of the `u_run.py`'s on an agent are run in parallel for speed.  However HW, and in certain cases third party tools limitations, mean that they are not always actually able to run in parallel.  Key examples of this are:

- test instances which share HW: for instance `13.0.0` (NRF5SDK GCC), `13.0.1` (NRF5SDK SES) and `13.1` (Zephyr) run on the same nRF52 board; this is ameliorated by the `DATABASE.md` instance configuration making sure that only one of these HW-sharing-instances runs the longer \[cellular\] tests,
- running the JLink tools on more than one thing at once on a given PC leads to intermittent failures, hence the `u_run_nrf5sdk.py` and `u_run_zephyr.py` scripts employ a "JLink lock" to prevent this happening,
- programming an STM32F4 board using the ST tools at the same time as monitoring the progress of another running STM32F4 board leads to intermittent failures, hence the `u_run_stm32f4.py` script ensures that all downloads to STM32F4 boards have completed before any STM32F4 test runs are allowed to commence,
- the ESP-IDF tools begin with an installation phase which requires access to an area on the hard disk of the agent that is shared between ESP-IDF instances and hence this phase of an ESP-IDF run cannot be executed at the same time as another (which `u_run_esp_idf.py` manages).

In all cases the Python scripts sort this out, however, how the instances that a `u_controller_client.py` wants to run are allocated to test agents has an impact on the overall test duration. `u_controller_client.py` is aware of the limitations above and uses that knowledge plus the expected duration of each test instance from `DATABASE.md` to spread the test load as optimally as it can across agents.

Test times are kept lowest if the NRF5SDK and Zephyr instances, the STM32F4 instances, and to some extent the ESP-IDF instances, are available on as many agents as possible so that they *can* be run at the same time.

# Running `u_controller_client.py`
Command-line help can be obtained by running `python u_controller_client.py -h`.  An example command-line might be:

`python u_controller_client.py -p 17010 -s summary.txt -d debug.txt -a http://nexus.blah.com:5600/archives -c fred:secret c1 https://github.com/u-blox/ubxlib/ #7cbb33169135dd341ce48c54aebf53ef3f04fc9a #a955d6d1b0d5a5f362b437fcfd57e26c000902d2 "some text" <a list of file paths, unquoted, separated with spaces>`

...where:

- `17010` is the port number to look for agents on (the default if this is omitted is to not filter by port number at all),
- `summary.txt` is the name to use for the summary files that the agent(s) should write,
- `debug.txt` is the name to use for the debug output files that the agent(s) should write,
- `http://nexus.blah.com:5600/archives` is the URL where all of the `summary.txt` and `debug.txt` files should be copied to,
- `fred:secret` are the credentials (if required) for the archive URL,
- `c1` is the name for this controller,
- `https://github.com/u-blox/ubxlib/` is the URL of the repo to test,
- `#7cbb33169135dd341ce48c54aebf53ef3f04fc9a` is the hash of `master` (the `#` character is not actually required but may be included for consistency with...),
- `#a955d6d1b0d5a5f362b437fcfd57e26c000902d2` is the hash of the commit to be tested; this may instead be a branch name (i.e. without a `#` at the start) in which case the HEAD revision of that branch will always be retrieved (since the agent has no way of knowing whether it already has HEAD of the branch or not); as this could require an update to the agent code the use of a specific hash is faster,
- `"some text"` is the text from the commit message of the code to be tested: this is parsed for a line that starts with `test: foo bar` in the same way as for `u_run_branch.py`; it is wise to process it to replace `"` with, say, a back-tick, and linefeeds with `\n` in order that the text survives passage through the command-line,
- `<a list of file paths, unquoted, separated with spaces>` is the space-separated list of changed file paths within the `ubxlib` repo between the two hashes (again the same as `u_run_branch.py`), which `Jenkinsfile` would normally obtain from `git`; if `master` is being tested the two hashes will be the same and this list of changed files would be omitted.

Hence a Jenkins server would typically grab a commit of the repo, determine the changed file list between that commit and `master`, execute the version of `u_controller_client.py` from that commit with a command-line something like the above to perform the tests and archive the results, displaying a link to the archive location for the user.  This is the same pattern as for MK I with `u_run_branch.py` except that, in MK I, the `Jenkinsfile` stage would be running directly on a single test agent; in the MK II case `Jenkinsfile` runs only on the Jenkins server and uses `u_controller_client.py` to get each `u_agent_service.py` to carry out all the fetch/run processes across multiple test agents in parallel, reducing the test time.

# Running `u_agent_service.py`
Command-line help can be obtained by running `python u_agent_service.py -h`.  An example command-line might be:

`python u_agent_service.py -p 17010 -u C:\agent\Unity -n test_agent_1 z: 0.0 0.1 0.2 2 3 4 13.0.0 17`

...where:

- `17010` is the port number to listen on (the default if this is omitted is 17003),
- `C:\agent\Unity` is a common copy of Unity which will be used by all instances and will be kept up to date by the agent,
- `test_agent_1` is a name for this test agent (if this is omitted the IP address of the PC on which the agent is running is used),
- `z:` is a working directory on the agent, where the code under test will be placed and the builds will be performed: note the use of a `subst`ed drive, again to keep path lengths as short as possible,
- `0.0 0.1 0.2 2 3 4 13.0.0 17` are the space-separated list of instances this agent is set up to support.

Hence a PC would be built to support a given set of instances (HW attached, tools installed, etc.), a version of `ubxlib` would initially be cloned into, say, `c:\agent` on this machine, then `CD` to the `c:\agent\ubxlib\port\platform\common\automation` directory and execute a line something like the above, likely at the end of the boot process for that PC for reliable start-up.

# An **IMPORTANT** Detail: Multiple `ubxlib`s
In the picture above the `ubxlib` repo exists in three different places:

1.  on the machine where the `u_controller_client.py` script is running: Jenkins will typically have fetched the repo to test it and will run the version of `u_controller_client.py` that it finds there,
2.  on the machine where `u_agent_service.py` is running, in an "agent" directory,
3.  also on the machine where `u_agent_service.py` is running but this time the version that `u_controller_client.py` has decided to test.

(2) and (3) are distinct and separate; `u_controller_client.py` is able to determine whether (2) on a given agent needs to be updated and will command `u_agent_service.py` to do this as necessary.

The **IMPORTANT** bit: one impact of this is that you need to be careful if you are pushing changes to `u_agent_service.py` or the things it calls/imports (e.g. `u_agent.py`): if you commit something that causes `u_agent_service.py` to stop even connecting then **all** of the test agents will stop working for everyone.  Should this happen you should fix the problem, commit it, push it, and then restart the agents with the new code (e.g. stop and restart `agent_start.bat` in the Windows Task Scheduler if you're using that platform/mechanism).