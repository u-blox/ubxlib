#!/usr/bin/env python

'''This originally from https://github.com/karlp/swopy/blob/master/swodecoder.py'''

from __future__ import print_function
from multiprocessing import Process, freeze_support # Needed to make Windows behave
                                                    # when doing multiprocessing,
import argparse
import time
import struct

# File read chunk size
CHUNK_SIZE = 1024

class ITMDWTPacket(object):
    pass

class SynchroPacket(ITMDWTPacket):
    def __repr__(self):
        return "SynchroPacket"

class OverflowPacket(ITMDWTPacket):
    def __repr__(self):
        return "OverflowPacket"

class SourcePacket(ITMDWTPacket):
    def __init__(self, address, source, size, data):
        self.address = address
        self.source = source
        self.size = size
        if size == 1:
            self.data = data[0]
        elif size == 2:
            data_bytes = bytearray(data)
            self.data = struct.unpack_from("<H", data_bytes)[0]
            self.sdata = struct.unpack_from("<h", data_bytes)[0]
        elif size == 4:
            data_bytes = bytearray(data)
            self.data = struct.unpack_from("<I", data_bytes)[0]
            self.sdata = struct.unpack_from("<i", data_bytes)[0]

    def __repr__(self):
        if self.size == 1:
            return "SourcePacket8(A={}, S={}, D={})".format(self.address,
                                                            self.source,
                                                            chr(self.data))
        if self.size == 2:
            return "SourcePacket16(A={}, S={}, D={} {:x}".format(self.address,
                                                                 self.source,
                                                                 self.data,
                                                                 self.data)
        if self.size == 4:
            return "SourcePacket32(A={}, S={}, D={} {:x}".format(self.address,
                                                                 self.source,
                                                                 self.data,
                                                                 self.data)

def coroutine(func):
    def start(*args, **kwargs):
        corout = func(*args, **kwargs)
        corout.next()
        return corout
    return start

@coroutine
def packet_parser(target, assume_sync=False):
    '''Process data and successively yield parsed ITM/DWT packets'''
    synchro = [0, 0, 0, 0, 0, 0x80]

    in_sync = assume_sync
    while True:
        frame = []
        while not in_sync:
            data_byte = yield
            if data_byte == 0:
                frame.append(data_byte)
            else:
                if (data_byte == 0x80) and (len(frame) == 5):
                    frame.append(data_byte)
                else:
                    print("Not in sync: invalid byte for sync frame: {}".   \
                          format(data_byte))
                    frame = []

            if frame == synchro:
                in_sync = True
                target.send(SynchroPacket())

        # OK, we're in sync now, need to be prepared for anything at all...
        data_byte = yield
        if data_byte == 0:
            fin = False
            frame = []
            while not fin:
                if data_byte == 0:
                    frame.append(data_byte)
                else:
                    if (data_byte == 0x80) and (len(frame) == 5):
                        frame.append(data_byte)
                    else:
                        print("invalid sync frame byte? trying to resync: {}".  \
                              format(data_byte))
                        frame = []
                if frame == synchro:
                    target.send(SynchroPacket())
                    fin = True
                else:
                    data_byte = yield
        elif data_byte & 0x3 == 0:
            # Trace packet type is overflow, timestamp or reserved
            if data_byte == 0x70:
                print("Overflow!")
                target.send(OverflowPacket())
            else:
                print("Protocol packet decoding not handled, breaking"         \
                      " stream to next sync :( byte was: {} {:x}".format(data_byte,
                                                                         data_byte))
                in_sync = False
        else:
            # Trace packet type is SWIT, i.e. from the application
            address = (data_byte & 0xf8) >> 3
            source = (data_byte & 0x4) >> 2
            plen = data_byte & 0x3
            rlen = zip([0, 1, 2, 3], [0, 1, 2, 4])[plen][1] # 1,2,4 byte mappings
            data = []
            for thing in range(rlen):
                del thing
                data_byte = yield
                data.append(data_byte)
            sources = SourcePacket(address, source, rlen, data)
            target.send(sources)

@coroutine
def simple_packet_parser(address, target):
    '''Process data simply, 'cos we know exactly what we send'''
    expecting_swit = True

    while True:
        # We're looking only for "address" (which can't be
        # -1 in this case) and we also know that CMSIS
        # only offers ITM_SendChar(), so packet length
        # is always 1, and we only send ASCII characters,
        # so the top bit of the data byte must be 0.
        # Hence, when we see SWIT (SoftWare Instrumentation Trace
        # I think, anyway, the bit that carries our prints
        # off the target) which is 0bBBBBB0SS, where BBBBB is
        # address and SS is 0x10, we know that the next
        # byte is probably data and if it is ASCII then
        # it is data.  Anything else is ignored.
        # The reason for doing it this way is that the
        # ARM ITM only sends out sync packets under
        # special circumstances so it is not a recovery
        # mechanism for simply losing a byte in the
        # transfer, which does happen occasionally.
        data_byte = yield
        if expecting_swit:
            if ((data_byte & 0x03) == 0x01) and                   \
               ((data_byte & 0xf8) >> 3 == address):
                # Trace packet type is SWIT, i.e. our
                # application logging
                _address = address
                source = (data_byte & 0x4) >> 2
                data = []
                expecting_swit = False
        else:
            if data_byte & 0x80 == 0:
                data.append(data_byte)
                sources = SourcePacket(_address, source, 0x01, data)
                target.send(sources)
            expecting_swit = True

@coroutine
def insane_verbose_packet_receiver():
    '''A simple co-routine "sink" for receiving full frames'''
    while True:
        frame = (yield)
        print("Got frame: {}".format(frame))

@coroutine
def packet_receiver_console_printer(valid_address=-1):
    while True:
        thing = yield
        if not hasattr(thing, "address"):
            # Skip things like synchro packets
            continue
        if (thing.address == valid_address) or valid_address == -1:
            if thing.size == 1:
                print(chr(thing.data), end='')
            else:
                print("Channel {}: {} byte value: {} : {:x} : {}".        \
                      format(thing.address, thing.size, thing.data,
                             thing.data, thing.sdata))

def main(file_name, address, sync, timeout):
    '''main() as a function'''
    inactivity_time_seconds = 0

    file_handle = open(file_name, "rb")
    if address != -1:
        # Use the simple packet parser because we know exactly what's
        # coming at us
        parser = simple_packet_parser(address, target=packet_receiver_console_printer(address))
    else:
        parser = packet_parser(target=packet_receiver_console_printer(address),
                               assume_sync=sync)

    with file_handle:
        while True:
            buf = file_handle.read(CHUNK_SIZE)
            if buf:
                inactivity_time_seconds = 0
                [parser.send(ord(item)) for item in buf]
            else:
                if timeout != 0:
                    time.sleep(0.1)
                    inactivity_time_seconds += 0.1
                    if (timeout > 0) and (inactivity_time_seconds > timeout):
                        break
                else:
                    break
    file_handle.close()

if __name__ == '__main__':
    freeze_support()

    ARG_PARSER = argparse.ArgumentParser()
    ARG_PARSER.add_argument("file_name", help="SWO binary file to parse.")
    ARG_PARSER.add_argument("--address", "-a", type=int, default=-1,
                            help="which channels to print, -1 for all.")
    ARG_PARSER.add_argument("--sync", "-s", action="store_true",
                            help="assume we have sync, don't wait for it.")
    ARG_PARSER.add_argument("--timeout", "-t", type=int, default=0,
                            help="exit if there is nothing to process from"      \
                                 " the pipe for this long (in seconds), specify" \
                                 " -1 to block forever, 0 to never wait.")
    ARGS = ARG_PARSER.parse_args()

    # This approach required to make Windows behave when this
    # module is called during multiprocessing
    # see https://docs.python.org/2/library/multiprocessing.html#windows
    PROCESS = Process(target=main, args=(ARGS.input_file_name, ARGS.address,
                                         ARGS.sync, ARGS.timeout))
    PROCESS.start()
