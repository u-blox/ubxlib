"""Log readers used by the log pytask commands to read out log from targets"""

import time
import os
from serial import Serial
from pylink import JLink, jlock, library
from pylink.enums import JLinkInterfaces

class URttReader:
    """A simple JLink RTT reader"""
    def __init__(self, device, jlink_serial=None,
                 jlink_logfile=None, reset_on_connect=False,
                 rtt_block_address=None):
        """
        device: The JLink target device to connect
        jlink_serial: Optional JLink serial number
        jlink_logfile: If you want to log the JLink com you can specify a logfile
        reset_on_connect: When set to True a target reset will be triggered on connect() call
        """
        self.device = device
        self.serial = jlink_serial
        try:
            # This is normally what we want to do...
            self.jlink = JLink()
        except:
            # ...except on ARM 64 bit where PyLink picks up a 32-bit library that is
            # hanging around in the SEGGER directory for some reason, so we have to
            # point it to the right location
            self.jlink = JLink(lib=library.Library(dllpath="/opt/SEGGER/JLink/libjlinkarm.so"))
        self.jlink_logfile = jlink_logfile
        self.reset_on_connect = reset_on_connect
        self.block_address = rtt_block_address

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, type_, value, traceback):
        self.close()

    def connect(self):
        """Connect JLink to the target"""

        if self.serial is not None:
            # This is a workaround for handling stale JLink locks that can happen
            # if system is rebooted during the time a JLink lock is held.
            # When this happens the file lock will point at a PID that was valid
            # before the reboot but may now be a service process etc after the reboot.
            lock = jlock.JLock(self.serial)
            if os.path.exists(lock.path):
                try:
                    os.remove(lock.path)
                except PermissionError:
                    # If the lock is currently held we will get PermissionError
                    # But we will do nothing here and let jlink.open() detect this instead
                    pass

        self.jlink.open(serial_no=self.serial)
        if self.jlink_logfile:
            self.jlink.set_log_file(self.jlink_logfile)
        print(f"Connecting to {self.device}")
        self.jlink.set_tif(JLinkInterfaces.SWD)
        self.jlink.connect(self.device)
        if self.reset_on_connect:
            print("Resetting target")
            self.jlink.reset(halt=False)
        print("Enabling RTT")
        self.jlink.rtt_start(self.block_address)

    def close(self):
        """Closes the JLink connection"""
        print("Disabling RTT")
        self.jlink.rtt_stop()
        print("Closing connection")
        self.jlink.close()

    def read(self, timeout=0.5):
        """Reads the JLink RTT buffer
        If there are no new data available until timeout hits
        this function will return None"""
        start_time = time.time()
        while time.time() - start_time < timeout:
            data = self.jlink.rtt_read(0, 4096)
            if len(data) > 0:
                # We got the data
                return data
            # No data - try again later
            time.sleep(0.1)
        return None


class UUartReader(Serial): # pylint: disable=too-many-ancestors
    """A PySerial wrapper that will set RTS and DTR state on open
    dtr_state: Set to True to turn on DTR on open, false to to turn off
               DTR on open. If not specified (or set to None) DTR will be
               left untouched on open.
    rts_state: Set to True to turn on RTS on open, false to to turn off
               RTS on open. If not specified (or set to None) RTS will be
               left untouched on open.
    """
    def __init__(self, *args, dtr_state=None, rts_state=None, **kwargs):
        self.dtr_state=dtr_state
        self.rts_state=rts_state
        super(UUartReader, self).__init__(*args, **kwargs)

    def open(self, *args, **kwargs):
        print(f"Opening {self.port}")
        super(UUartReader, self).open(*args, **kwargs)
        if self.dtr_state is not None:
            print("Setting DTR {}".format("on" if self.dtr_state else "off"))
            self.dtr = self.dtr_state
        if self.rts_state is not None:
            print("Setting RTS {}".format("on" if self.rts_state else "off"))
            self.rts = self.rts_state
