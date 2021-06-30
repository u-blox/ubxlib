#!/usr/bin/env python

'''Process checker; the other half of u_process_wrapper.py.'''

import sys
from signal import signal, SIGTERM, SIGBREAK
from time import sleep, gmtime, strftime

MY_NAME = "enwrapped"

FILE = None

def log(string):
    string = "{} {}: {}".format(strftime("%Y-%m-%d_%H:%M:%S", gmtime()), MY_NAME, string)
    print(string, flush=True)
    if FILE:
        FILE.write(string + "\n")
        FILE.flush()

def sigterm_break():
    ''' Exit on SIGBREAK'''
    log("received SIGBREAK, exiting...")
    sys.exit(-32)

def sigterm_handler():
    ''' Ignore SIGTERM'''
    log("ignoring SIGTERM...")

if __name__ == "__main__":

    signal(SIGTERM, sigterm_handler)
    signal(SIGBREAK, sigterm_handler)

    COUNT = 0
    FILE = open("{}.txt".format(MY_NAME), "w+")

    try:

        while COUNT < 30:
            COUNT += 1
            log("wait {}".format(COUNT))
            sleep(1)

    except KeyboardInterrupt:
        log("ignoring CTRL-C")

    FILE.close()

    sys.exit(0)
