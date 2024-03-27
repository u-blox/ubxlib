#!/usr/bin/env python

'''Parse ubxlib log output to obtain the GNSS UBX traffic and write it to uCenter.'''

from multiprocessing import Process, freeze_support # Needed to make Windows behave
                                                    # when run under multiprocessing,
from time import sleep
import os
import serial
import platform # Figure out current OS
import sys # For exit() and stdout
import argparse

if platform.system() == "Linux":
    # termios, which pty requires, is only available on Linux
    import pty

# This script can be fed ubxlib log output and will find in it
# the traffic between ubxlib and the GNSS device which it
# can turn into UBX-format messages that the u-blox uCenter
# tool is able to display.
#
# The input to this script can be from a file, which will be
# output to another file, or it can be from a stream (e.g.
# directly from a UART port carrying the ubxlib log output)
# which will be output to a virtual COM port to which uCenter
# can be connected.
#
# ubxlib will emit log output by default and, if you have
# opened the GNSS device/network with the `uDevice`/`uNetwork`
# API, this will include the GNSS traffic by default; otherwise
# you should enable logging of GNSS traffic by calling
# uGnssSetUbxMessagePrint() with true.

# The characters at the start of a GNSS message received from
# the GNSS device.  This matches
# U_GNSS_PRIVATE_SENT_MESSAGE_STRING in gnss/src/u_gnss_private.c
GNSS_LOG_LINE_MARKER_RECEIVE = "U_GNSS: decoded UBX response"

# The characters at the start of a GNSS message transmitted to
# the GNSS device.  This matches the start of
# U_GNSS_PRIVATE_RECEIVED_MESSAGE_STRING_WITH_CLASS_AND_ID in
# gnss/src/u_gnss_private.c
GNSS_LOG_LINE_MARKER_COMMAND = "U_GNSS: sent command"

# Default output file extension
OUTPUT_FILE_EXTENSION = "ubx"

# The default baud rate if a serial port is used
BAUD_RATE = 9600

def line_parse_gnss_receive_message(line_number, input_line, start_index):
    '''Parse a log line containing UBX message received from a GNSS device'''
    message = bytearray()

    # A line containing a response received from the GNSS device looks like this:
    #
    # U_GNSS: decoded UBX response 0x0a 0x06: 01 05 00 ...[body 120 byte(s)].
    #
    # ...i.e. the message class/ID followed by the body, missing out the header, the
    # length and the FCS, so we will recreate those
    #
    input_string = input_line[start_index:]
    # Capture the message class and ID
    class_and_id = []
    try:
        class_and_id = [int(i, 16) for i in input_string[:10].split(" 0x") if i]
    except ValueError:
        pass
    if len(class_and_id) == 2:
        # Find the length
        body_length = []
        body_index = input_string.find("body ")
        if body_index >= 0:
            body_length = [int(i) for i in input_string[body_index:].split() if i.isdigit()]
            if len(body_length) == 1:
                # Assemble the binary message, starting with the header
                message.append(0xb5)
                message.append(0x62)
                # Then the class and ID
                message.append(int(class_and_id[0]))
                message.append(int(class_and_id[1]))
                # Then the little-endian body length
                message.append(body_length[0] % 256)
                message.append(int(body_length[0] / 256))
                # Now the body
                try:
                    message.extend([int(i, 16) for i in input_string[11:11 + (body_length[0] * 3)].split() if i])
                except ValueError:
                    print(f"Warning: found non-hex value in body of decoded line {line_number}: \"{input_line}\".")
                # Having done all that, work out the FCS and append it
                fcs_ca = 0
                fcs_cb = 0
                for integer in message[2:]:
                    fcs_ca += integer
                    fcs_cb += fcs_ca
                message.append(int(fcs_ca) & 0xFF)
                message.append(int(fcs_cb) & 0xFF)
            else:
                print(f"Warning: couldn't find body length in decoded line {line_number}: \"{input_line}\".")
        else:
            print(f"Warning: couldn't find \"body\" in decoded line {line_number}: \"{input_line}\".")
    else:
        print(f"Warning: couldn't find message class/ID in decoded line {line_number}: \"{input_line}\".")

    return message

def line_parse_gnss_command_message(line_number, input_line, start_index):
    '''Parse a log line containing UBX message sent to a GNSS device'''
    message = bytearray()

    # A line containing a command sent to the GNSS device looks like this:
    #
    # U_GNSS: sent command b5 62 06 8a 09 00 00 01 00 00 21 00 11 20 08 f4 51.
    #
    # ...i.e. it contains the whole thing, raw, so nice and easy
    input_string = input_line[start_index:]
    try:
        message.extend([int(i[:2], 16) for i in input_string.split() if i])
    except ValueError:
        print(f"Warning: found non-hex value in body of sent line {line_number}: \"{input_line}\".")

    return message

def line_parser(line_number, input_line, responses_only):
    '''Parse a line and return a binary messsage'''
    message = None
    start_index = input_line.find(GNSS_LOG_LINE_MARKER_RECEIVE)
    if start_index >= 0:
        start_index += len(GNSS_LOG_LINE_MARKER_RECEIVE)
        message = line_parse_gnss_receive_message(line_number,
                                                  input_line,
                                                  start_index)
    else:
        if not responses_only:
            start_index = input_line.find(GNSS_LOG_LINE_MARKER_COMMAND)
            if start_index >= 0:
                start_index += len(GNSS_LOG_LINE_MARKER_COMMAND)
                message = line_parse_gnss_command_message(line_number,
                                                          input_line,
                                                          start_index)
    return message

def line_read_character_by_character(input_handle):
    '''Read a whole line from a character device'''
    return_value = None
    line = ""
    eol = False
    try:
        while not eol and line is not None:
            buf = input_handle.read(1)
            if buf:
                character = buf.decode('ascii', errors='backslashreplace')
                eol = character == "\n"
                if not eol:
                    line = line + character
            else:
                line = None
                # Since this is a busy/wait we sleep a bit if there is no data
                # to offload the CPU
                sleep(0.01)
        if eol:
            line = line.rstrip()
    except UnicodeDecodeError:
        # Just ignore it.
        pass
    return_value = line

    if return_value is None or return_value == "":
        # Sleep a bit to offload the CPU
        sleep(0.01)

    return return_value

def main(source, destination, responses_only, echo_on, baud_rate):
    '''Main as a function'''
    return_value = 1

    # Open the source
    reading_from_device = False
    source_handle = None
    if os.path.isfile(source):
        try:
            source_handle = open(source, "r", encoding="utf8")
        except (NameError, FileNotFoundError, PermissionError) as ex:
            print(f"{type(ex).__name__} while trying to open \"{source}\".")
    else:
        print(f"Input \"{source}\" is not a file, trying to open it as a device.")
        try:
            # Open what is assumed to be a serial device
            source_handle = serial.Serial(baudrate=baud_rate, timeout=0.05)
            source_handle.port = source
            source_handle.open()
            reading_from_device = True
        except (ValueError, serial.SerialException) as ex:
            print(f"{type(ex).__name__} while opening device \"{source}\".")
            source_handle = None
    if source_handle:
        # Open either the output file or a virtual
        # device that the caller can connect uCenter to
        destination_handle = None
        writing_to_device = False
        try:
            if destination:
                if os.path.isfile(destination):
                    if not os.path.splitext(destination)[1]:
                        destination += "." + OUTPUT_FILE_EXTENSION
                    destination_handle = open(destination, "wb")
                else:
                    print(f"Output \"{destination}\" is not a file, trying to open it as a device.")
                    try:
                        # Open what is assumed to be a serial device
                        destination_handle = serial.Serial(baudrate=baud_rate, timeout=0.05)
                        destination_handle.port = destination
                        destination_handle.open()
                        writing_to_device = True
                    except (ValueError, serial.SerialException) as ex:
                        print(f"{type(ex).__name__} while opening device \"{destination}\".")
                        destination_handle = None
            else:
                if reading_from_device and platform.system() == "Linux":
                    try:
                        # Try to open a virtual serial port
                        # that we can write the output to
                        _, device_output = pty.openpty()
                        destination = os.ttyname(device_output)
                        destination_handle = serial.Serial(destination)
                        writing_to_device = True
                    except (NameError, FileNotFoundError, PermissionError) as ex:
                        print(f"{type(ex).__name__} while trying to create virtual device" \
                              f" for output.")
                if not destination_handle:
                    # Output file name is source file with modified extension
                    destination = os.path.splitext(source)[0] + "." + OUTPUT_FILE_EXTENSION
                    destination_handle = open(destination, "wb")
            if destination_handle:
                print_text = f"Reading from {source}"
                if destination:
                    print_text += f", writing to {destination}"
                print_text += f", looking for lines containing \"{GNSS_LOG_LINE_MARKER_RECEIVE}\""
                if not responses_only:
                    print_text += f" and \"{GNSS_LOG_LINE_MARKER_COMMAND}\"..."
                print(print_text)
                if reading_from_device:
                    print("Use CTRL-C to stop.")
                line_number = 0
                message_count = 0
                try:
                    while True:
                        if reading_from_device:
                            # Otherwise do a character-by-character read
                            # from what is probably a serial device
                            source_line = line_read_character_by_character(source_handle)
                        else:
                            # If we have a file we can readline()
                            source_line = source_handle.readline()
                            # Strip off the line ending
                            source_line = source_line.rstrip()
                        if source_line:
                            if echo_on:
                                print(source_line)
                            # Parse and write the line to the output
                            line_number += 1
                            message = line_parser(line_number, source_line, responses_only)
                            if message:
                                message_count += 1
                                if destination_handle:
                                    destination_handle.write(message)
                        else:
                            if not reading_from_device:
                                # If we are reading from a file this
                                # must be the end of the file
                                break;
                except KeyboardInterrupt as ex:
                    print("CTRL-C pressed.")
                    pass
                print(f"Found {message_count} UBX messages(s) in {line_number} line(s).")
                source_handle.close()
                if destination and not writing_to_device and message_count > 0:
                    print(f"File {destination} has been written: you may open it in uCenter.")
                return_value = 0
            if destination_handle:
                destination_handle.close()
        except (NameError, FileNotFoundError, PermissionError) as ex:
            print(f"{type(ex).__name__} while trying to open {destination} for writing.")
    else:
        print(f"Unable to open \"{source}\".")

    return return_value

if __name__ == "__main__":
    PARSER = argparse.ArgumentParser(description="A script to"      \
                                     " find UBX-format GNSS"        \
                                     " traffic in ubxlib log"       \
                                     " output and write it to"      \
                                     " something that uCenter can"  \
                                     " open.\n")
    PARSER.add_argument("source", help="the source of ubxlib log"    \
                        " data; either a file or a device that is"  \
                        " emitting ubxlib log output.")
    PARSER.add_argument("destination", nargs="?", help= "optional"  \
                        " output name; if provided then the output" \
                        " will be written to this file or device"   \
                        " (if the file exists it will be"           \
                        " overwritten, if no extension is given ." +\
                        OUTPUT_FILE_EXTENSION + " will be added)."  \
                        " If this parameter is omitted and the"     \
                        " source is a device then, on Linux, the"   \
                        " output will be written to a PTY."         \
                        " Otherwise the output will be written to"  \
                        " the same name as the source but with"     \
                        " extension " + OUTPUT_FILE_EXTENSION + ".")
    PARSER.add_argument("-r", action="store_true", help="include"   \
                        " only the responses from the GNSS device"  \
                        " (i.e. leave out any commands sent to the" \
                        " GNSS device).")
    PARSER.add_argument("-e", action="store_true", help="echo all"  \
                        " input to stdout while working.")
    PARSER.add_argument("-b", type=int, default=BAUD_RATE, help=    \
                        "if the source is a serial port use this"    \
                        " baud rate.")

    ARGS = PARSER.parse_args()

    # Call main()
    RETURN_VALUE = main(ARGS.source, ARGS.destination, ARGS.r, ARGS.e, ARGS.b)

    sys.exit(RETURN_VALUE)

# A main is required because Windows needs it in order to
# behave when this module is called during multiprocessing
# see https://docs.python.org/2/library/multiprocessing.html#windows
if __name__ == '__main__':
    freeze_support()
    PROCESS = Process(target=main)
    PROCESS.start()
