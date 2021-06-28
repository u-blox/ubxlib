import time
import pylink

class URttReader:
    """A simple JLink RTT reader"""
    def __init__(self, device, jlink_serial=None,
                 jlink_logfile=None, printer=None, prompt=""):
        """device: The JLink device to connect"""
        self.device = device
        self.serial = jlink_serial
        self.jlink = pylink.JLink()
        self.jlink_logfile = jlink_logfile
        self.printer = printer
        self.prompt = prompt
        self.print_read_time = time.time()
        self.buffer = bytearray()

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, type_, value, traceback):
        self.close()

    def _print(self, string):
        string = "{}J-Link S/N {}: {}".format(self.prompt, self.serial, string)
        if self.printer:
            self.printer.string(string)
        else:
            print(string)

    def connect(self):
        self.jlink.open(self.serial)
        if self.jlink_logfile:
            self.jlink.set_log_file(self.jlink_logfile)
        self._print("Connecting to {}".format(self.device))
        self.jlink.set_tif(pylink.enums.JLinkInterfaces.SWD)
        self.jlink.connect(self.device)
        self._print("Enabling RTT")
        self.jlink.rtt_start(None)

    def close(self):
        """Closes the JLink connection"""
        self._print("Disabling RTT")
        self.jlink.rtt_stop()
        self._print("Closing connection")
        self.jlink.close()

    def read(self, length):
        """Reads the JLink RTT buffer
           Mimics the read() from PySerial"""
        if time.time() - self.print_read_time > 60:
            self._print("Still reading")
            self.print_read_time = time.time()

        # When JLink logging is enabled all calls to
        # rtt_read() will be logged. For this reason
        # we have added a buffer here that act as a FIFO.
        if length > len(self.buffer):
            # Only when the amount of data that the caller
            # wants to read is not available we do the actual
            # RTT reading
            new_data = self.jlink.rtt_read(0, 1024)
            if len(new_data) > 0:
                self.buffer.extend(new_data)

        if length > len(self.buffer):
            length = len(self.buffer)

        if length == 0:
            # Add some slack when there is no data available
            # Don't worry - the JLink DLL will continue to poll
            # and buffer data even when we are not calling rtt_read()
            time.sleep(0.5)
            return None

        # Fetch the data from our FIFO buffer
        data = self.buffer[:length]
        # Delete the data we have read
        self.buffer[:length] = b''
        return data
