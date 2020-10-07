#!/usr/bin/env python

'''Manage connections for ubxlib testing.'''

from time import sleep
import u_utils

# The default guard time for waiting for a connection lock in seconds
CONNECTION_LOCK_GUARD_TIME_SECONDS = (60 * 30)

CONNECTION_LIST = [None,      # Instance 0, Lint, no connection, no need for a lock
                   None,      # Instance 1, Doxygen, no connection, no need for a lock
                   None,      # Instance 2, AStyle checker, no connection, no need for a lock
                   None,      # Instance 3, Pylint, no connection, no need for a lock
                   None,      # Instance 4, reserved
                   None,      # Instance 5, reserved
                   None,      # Instance 6, reserved
                   None,      # Instance 7, reserved
                   None,      # Instance 8, reserved
                   None,      # Instance 9, reserved
                   # Instance 10, WHRE board
                   {"lock": None, "serial_port": "COM10", "debugger": None},
                   # Instance 11, ESP32, SARA-R412M-02B
                   {"lock": None, "serial_port": "COM8", "debugger": None},
                   # Instance 12, ESP32, SARA-R5
                   {"lock": None, "serial_port": "COM9", "debugger": None},
                   # Instance 13, NRF52, SARA-R5
                   {"lock": None, "serial_port": "COM6", "debugger": "683253856",
                    "swo_port": u_utils.JLINK_SWO_PORT},
                   # Instance 14, C030-R412M (STM32F4), live network GPRS
                   {"lock": None, "serial_port": "COM14", "debugger": "066EFF535154887767012236",
                    "swo_port": u_utils.STLINK_SWO_PORT,
                    "gdb_port": u_utils.STLINK_GDB_PORT},
                   # Instance 15, C030-R412M (STM32F4), cat-M1
                   {"lock": None, "serial_port": "COM3", "debugger": "066DFF535154887767012834",
                    "swo_port": u_utils.STLINK_SWO_PORT + 1,
                    "gdb_port": u_utils.STLINK_GDB_PORT + 1},
                   # Instance 16, STM32F4 Discovery, SARA-R5
                   {"lock": None, "serial_port": "COM5", "debugger": "0668FF383032534E43184418",
                    "swo_port": u_utils.STLINK_SWO_PORT + 2,
                    "gdb_port": u_utils.STLINK_GDB_PORT + 2},
                   # Instance 17, NRF52, SARA-R412M-02B
                   {"lock": None, "serial_port": "COM7", "debugger": "683920969",
                    "swo_port": u_utils.JLINK_SWO_PORT + 3},
                   # Instance 18, C208 with SARA-R412M-02B connected via Segger J-Link box
                   {"lock": None, "serial_port": "COM12", "debugger": "50102100",
                    "swo_port": u_utils.JLINK_SWO_PORT + 4},
                   # Instance 19, C030-U201 board (STM32F4), live network 3G
                   {"lock": None, "serial_port": "COM4", "debugger": "066FFF565053787567193329",
                    "swo_port": u_utils.STLINK_SWO_PORT + 5,
                    "gdb_port": u_utils.STLINK_GDB_PORT + 3},
                   # Instance 20, NRF53: the COM port is the lowest numbered of the three
                   {"lock": None, "serial_port": "COM15", "debugger": "960115898",
                    "swo_port": u_utils.JLINK_SWO_PORT + 6}]

def init_locks(manager):
    '''Create locks'''
    for connection in CONNECTION_LIST:
        if connection and "lock" in connection:
            connection["lock"] = manager.RLock()

def get_lock(instance):
    '''Get the lock for an instance: does NOT lock it!!!'''
    _lock = None

    if instance[0] < len(CONNECTION_LIST):
        if CONNECTION_LIST[instance[0]] and "lock" in CONNECTION_LIST[instance[0]]:
            _lock = CONNECTION_LIST[instance[0]]["lock"]

    return _lock

def get_connection(instance):
    '''Return the connection for the given instance'''
    connection = None

    if instance[0] < len(CONNECTION_LIST):
        connection = CONNECTION_LIST[instance[0]]

    return connection

def get_instance(wanted_connection):
    '''Return the instance for the given connection'''
    instance = []

    if wanted_connection:
        for idx, connection in enumerate(CONNECTION_LIST):
            # Use try/catch in case just one side is None
            try:
                if (connection["serial_port"] == wanted_connection["serial_port"]) and \
                    (connection["debugger"] == wanted_connection["debugger"]):
                    instance.append(idx)
                    break
            except TypeError:
                pass
    return instance

# Note: it seems strange to be passing the connection_lock
# in here as it's already in the table above.  However, the
# locks need to work across process pools and so they
# need to be passed in from the caller, the storage above
# is for the caller only, if you call it from a different process
# you'll get another copy which will contain no lock.
def lock(connection, connection_lock, guard_time_seconds,
         printer, prompt):
    '''Lock the given connection'''
    timeout_seconds = guard_time_seconds
    success = False

    if connection:
        instance_text = u_utils.get_instance_text(get_instance(connection))
        if connection_lock:
            # Wait on the lock
            printer.string("{}instance {} waiting up to {} second(s)"   \
                           " to lock connection...".                    \
                           format(prompt, instance_text, guard_time_seconds))
            count = 0
            while not connection_lock.acquire(False) and                \
                ((guard_time_seconds == 0) or (timeout_seconds > 0)):
                sleep(1)
                timeout_seconds -= 1
                count += 1
                if count == 30:
                    printer.string("{}instance {} still waiting {} second(s)"     \
                                   " for a connection lock (locker is"            \
                                   " currently {}).".                             \
                                   format(prompt, instance_text, timeout_seconds,
                                          connection_lock))
                    count = 0
            if (guard_time_seconds == 0) or (timeout_seconds > 0):
                success = True
                printer.string("{}instance {} has locked a connection ({}).". \
                               format(prompt, instance_text, connection_lock))
        else:
            success = True
            printer.string("{}note: instance {} lock is empty.".           \
                           format(prompt, instance_text))

    return success

def unlock(connection, connection_lock, printer, prompt):
    '''Unlock the given connection'''

    if connection:
        instance_text = u_utils.get_instance_text(get_instance(connection))
        if connection_lock:
            connection_lock.release()
            printer.string("{}instance {} has unlocked a connection.".   \
                           format(prompt, instance_text))

class Lock():
    '''Hold a lock as a "with:"'''
    def __init__(self, connection, connection_lock, guard_time_seconds,
                 printer, prompt):
        self._connection = connection
        self._connection_lock = connection_lock
        self._guard_time_seconds = guard_time_seconds
        self._printer = printer
        self._prompt = prompt
        self._locked = False
    def __enter__(self):
        self._locked = lock(self._connection, self._connection_lock,
                            self._guard_time_seconds, self._printer,
                            self._prompt)
        return self._locked
    def __exit__(self, _type, value, traceback):
        del _type
        del value
        del traceback
        if self._locked:
            unlock(self._connection, self._connection_lock,
                   self._printer, self._prompt)