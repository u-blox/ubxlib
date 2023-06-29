'''This script is used by vscode to get the default build directory.'''

import os
import sys

DEFAULT_BUILD_DIR = os.getenv("UBXLIB_BUILD_DIR") or os.path.join(
    os.path.abspath(os.path.realpath(os.path.dirname(__file__)) + "/../../../../.."), ".vscode")
DEFAULT_BUILD_DIR = os.path.join(DEFAULT_BUILD_DIR, "_build")

def main():
    '''Script entrypoint'''
    global DEFAULT_BUILD_DIR
    if len(sys.argv) > 1:
        DEFAULT_BUILD_DIR = os.path.realpath(os.path.join(DEFAULT_BUILD_DIR, ".."))
    if not os.path.exists(DEFAULT_BUILD_DIR):
        os.makedirs(DEFAULT_BUILD_DIR)

    print(DEFAULT_BUILD_DIR)
if __name__ == '__main__':
    main()
