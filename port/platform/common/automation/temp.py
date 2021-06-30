#!/usr/bin/env python

'''Process wrapper to help with Jenkins integration; a version of Morné Joubert's great idea.'''

import sys
from signal import signal, SIGTERM, SIGBREAK
import subprocess
import threading
from time import sleep, gmtime, strftime

MY_NAME = "wrapper"

FILE = None

def log(string):
    string = "{} {}: {}".format(strftime("%Y-%m-%d_%H:%M:%S", gmtime()), MY_NAME, string)
    print(string)
    if FILE:
        FILE.write(string + "\n")
        FILE.flush()

def sigterm_break():
    ''' Exit on SIGBREAK'''
    log("received SIGBREAK, exiting...")
    if FILE:
        FILE.close()
    sys.exit(-22)

def sigterm_handler():
    ''' Exit on SIGTERM'''
    log("received SIGTERM, exiting...")
    if FILE:
        FILE.close()
    sys.exit(-2)

def process_read(process):
    '''Read output from a process and log it'''
    while process.poll() is None:
        string = process.stdout.readline().decode()
        log(string.rstrip())

if __name__ == "__main__":

    signal(SIGTERM, sigterm_handler)
    signal(SIGBREAK, sigterm_handler)

    FILE = open("{}.txt".format(MY_NAME), "w+")

    try:
        CREATION_FLAGS = 0
        #CREATION_FLAGS |= subprocess.CREATE_NO_WINDOW
        #CREATION_FLAGS |= subprocess.DETACHED_PROCESS
        #CREATION_FLAGS |= subprocess.CREATE_NEW_CONSOLE
        CREATION_FLAGS |= subprocess.CREATE_NEW_PROCESS_GROUP
        #CREATION_FLAGS |= subprocess.CREATE_BREAKAWAY_FROM_JOB
        PROCESS = subprocess.Popen(["cmd", "/c", "start", "/B", "python", "temp1.py"],
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT,
                                   shell=True,
                                   creationflags=CREATION_FLAGS)

        PROCESS_READ_THREAD = threading.Thread(target=process_read,
                                               args=(PROCESS,))
        PROCESS_READ_THREAD.start()

        while PROCESS.poll() is None:
            log("process is running.")
            sleep(1)

        while True:
            log("waiting for the end.")
            sleep(1)

    except ValueError as ex:
        log("ERROR: {} while trying to execute {}.".format(type(ex).__name__, str(ex)))
    except KeyboardInterrupt:
        log("received CTRL-C, exiting...")

    FILE.close()

    sys.exit(0)
