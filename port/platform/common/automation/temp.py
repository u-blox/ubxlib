#!/usr/bin/env python

'''Process wrapper to help with Jenkins integration; a version of Morné Joubert's great idea.'''

import sys
from signal import signal, SIGTERM, SIGBREAK
from time import sleep

def sigterm_break():
    ''' Just exit on receipt of SIGBREAK'''
    print("{}received SIGBREAK, exiting.".format(PROMPT))
    sys.exit(-12)

def sigterm_handler():
    ''' Just exit on receipt of SIGTERM'''
    print("{}received SIGTERM, exiting.".format(PROMPT))
    sys.exit(-2)

if __name__ == "__main__":
    # Trap SIGERM, which Jenkins sends
    signal(SIGTERM, sigterm_handler)

    # Trap SIGBREAK
    signal(SIGBREAK, sigterm_handler)

    try:
        while(True):
            print("WAITING FOR THE END.")
            sleep(5)
    except KeyboardInterrupt:
        print("{}received CTRL-C, exiting.")

    sys.exit(0)
