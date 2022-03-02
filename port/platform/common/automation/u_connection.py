#!/usr/bin/env python

'''Manage connections for ubxlib testing.'''

from time import sleep
import u_utils
import u_settings

# The default guard time for waiting for a connection lock in seconds
CONNECTION_LOCK_GUARD_TIME_SECONDS = (60 * 45)

# The connection for each instance
CONNECTION_LIST = [
    None,      # Instance 0, Lint, no connection, no need for a lock
    None,      # Instance 1, Doxygen, no connection, no need for a lock
    None,      # Instance 2, AStyle checker, no connection, no need for a lock
    None,      # Instance 3, Pylint, no connection, no need for a lock
    None,      # Instance 4, run static size check, no connection, no need for a lock
    None,      # Instance 5, run no floating point check, no connection, no need for a lock
    None,      # Instance 6, reserved
    None,      # Instance 7, reserved
    None,      # Instance 8, reserved
    None,      # Instance 9, reserved
    # Instance 10, WHRE board
    {"lock": None, "id": 10, **u_settings.CONNECTION_INSTANCE_10},
    # Instance 11, ESP32
    {"lock": None, "id": 11, **u_settings.CONNECTION_INSTANCE_11},
    # Instance 12, ESP32, SARA-R5
    {"lock": None, "id": 12, **u_settings.CONNECTION_INSTANCE_12},
    # Instance 13, NRF52840, SARA-R5
    {"lock": None, "id": 13, **u_settings.CONNECTION_INSTANCE_13},
    # Instance 14, STM32F4 Discovery, SARA-R412M-02B
    {"lock": None, "id": 14, **u_settings.CONNECTION_INSTANCE_14},
    # Instance 15, NRF52840, SARA-R410M-02B
    {"lock": None, "id": 15, **u_settings.CONNECTION_INSTANCE_15,
     "swo_port": u_utils.JLINK_SWO_PORT + 1},
    # Instance 16, C030-U201 board (STM32F4), live network 3G
    {"lock": None, "id": 16, **u_settings.CONNECTION_INSTANCE_16},
    # Instance 17, NRF5340: the COM port is the lowest numbered of the three
    {"lock": None, "id": 17, **u_settings.CONNECTION_INSTANCE_17,
     "swo_port": u_utils.JLINK_SWO_PORT + 2},
    # Instance 18, NRF5340 with SARA-R5 EVK: the COM port is the middle of the three
    {"lock": None, "id": 18, **u_settings.CONNECTION_INSTANCE_18,
     "swo_port": u_utils.JLINK_SWO_PORT + 3},
    # Instance 19, C030-R5 board (STM32F4), cat-M1
    {"lock": None, "id": 19, **u_settings.CONNECTION_INSTANCE_19},
    # Instance 20, WHRE board
    {"lock": None, "id": 20, **u_settings.CONNECTION_INSTANCE_20},
    # Instance 21, WHRE board
    {"lock": None, "id": 21, **u_settings.CONNECTION_INSTANCE_21},
    # Instance 22, ESP32, SARA-R422
    {"lock": None, "id": 22, **u_settings.CONNECTION_INSTANCE_22},
    # Instance 23, WINDOWS, SARA-R5
    # Note: windows has no serial port etc. but we need the lock field to permit
    # parallel operations.
    {"lock": None, "id": 23}
]

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
                if connection["id"] == wanted_connection["id"]:
                    instance.append(idx)
                    break
            except TypeError:
                pass
    return instance

def get_kmtronic(wanted_connection):
    '''Return the kmtronic HW reset mechanism for the given connection'''
    kmtronic = {}

    if wanted_connection:
        for connection in enumerate(CONNECTION_LIST):
            # Use try/catch in case just one side is None
            try:
                if connection["id"] == wanted_connection["id"]:
                    # "kmtronic" should be an object that includes
                    # "ip_address" and "hex_bitmap"
                    if "kmtronic" in wanted_connection and wanted_connection["kmtronic"] and \
                        wanted_connection["kmtronic"]["ip_address"] and \
                        wanted_connection["kmtronic"]["hex_bitmap"]:
                        kmtronic = wanted_connection["kmtronic"]
                    break
            except TypeError:
                pass
    return kmtronic

def get_usb_cutter_id_str(wanted_connection):
    '''Return the Cleware USB cutter ID string for the given connection'''
    usb_cutter_id_str = ""

    if wanted_connection:
        for connection in enumerate(CONNECTION_LIST):
            # Use try/catch in case just one side is None
            try:
                if connection["id"] == wanted_connection["id"]:
                    if "usb_cutter_id_str" in wanted_connection:
                        # "usb_cutter_id_str" for a Cleware USB cutter
                        # should be something like "1750665"
                        usb_cutter_id_str = wanted_connection["usb_cutter_id_str"]
                    break
            except TypeError:
                pass
    return usb_cutter_id_str

# Note: it seems strange to be passing the connection_lock
# in here as it is already in the table above.  However, the
# locks need to work across process pools and so they
# need to be passed in from the caller, the storage above
# is for the caller only, if you call it from a different process
# you'll get another copy which will contain no lock.
def lock(connection, connection_lock, guard_time_seconds,
         printer, prompt, keep_going_flag=None, hw_reset=True):
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
            while not connection_lock.acquire(False) and                 \
                ((guard_time_seconds == 0) or (timeout_seconds > 0)) and \
                u_utils.keep_going(keep_going_flag, printer, prompt):
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
                printer.string("{}instance {} has locked a connection ({}).". \
                               format(prompt, instance_text, connection_lock))
                if hw_reset:
                    kmtronic = get_kmtronic(connection)
                    usb_cutter_id_str = get_usb_cutter_id_str(connection)
                    if kmtronic:
                        printer.string("{}using KMTronic to reset {}...". \
                                       format(prompt, instance_text))
                        u_utils.kmtronic_reset(kmtronic["ip_address"], kmtronic["hex_bitmap"],
                                               printer, prompt)
                    if usb_cutter_id_str:
                        printer.string("{}using USB cutter to reset {}...". \
                                       format(prompt, instance_text))
                        u_utils.usb_cutter_reset([usb_cutter_id_str], printer, prompt)
                success = True
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
                 printer, prompt, keep_going_flag=None):
        self._connection = connection
        self._connection_lock = connection_lock
        self._guard_time_seconds = guard_time_seconds
        self._printer = printer
        self._prompt = prompt
        self._keep_going_flag = keep_going_flag
        self._locked = False
    def __enter__(self):
        self._locked = lock(self._connection, self._connection_lock,
                            self._guard_time_seconds, self._printer,
                            self._prompt, self._keep_going_flag)
        return self._locked
    def __exit__(self, _type, value, traceback):
        del _type
        del value
        del traceback
        if self._locked:
            unlock(self._connection, self._connection_lock,
                   self._printer, self._prompt)
