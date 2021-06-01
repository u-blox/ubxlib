import time
import pylink

class URttReader:
    """A simple JLink RTT reader"""
    def __init__(self, device, jlink_serial=None):
        """device: The JLink device to connect"""
        self.device = device
        self.serial = jlink_serial
        self.jlink = pylink.JLink()

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, type_, value, traceback):
        self.close()

    def connect(self):
        self.jlink.open(self.serial)
        print("connecting to %s..." % self.device)
        self.jlink.set_tif(pylink.enums.JLinkInterfaces.SWD)
        self.jlink.connect(self.device)
        self.jlink.rtt_start(None)

    def close(self):
        """Closes the JLink connection"""
        self.jlink.close()

    def read(self, length):
        """Reads the JLink RTT buffer
           Mimics the read() from PySerial"""
        buf = self.jlink.rtt_read(0, length)
        if len(buf) == 0:
            time.sleep(0.01)
            return None
        return bytes(buf)
