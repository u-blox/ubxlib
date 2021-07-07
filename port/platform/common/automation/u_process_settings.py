#!/usr/bin/env python

'''Settings common to u_process_wrapper.py and u_process_checker.py.'''

# The name of u_process_wrapper.py.
WRAPPER_NAME = "u_process_wrapper.py"

# The name of u_process_c.py.
CHECKER_NAME = "u_process_checker.py"

# The default port number to use
PORT = 50123

# What to use to invoke Python
PYTHON = "python"

# The special prefix used to pick a return value
# out of the socket streamed from process checker
RETURN_VALUE_PREFIX = "#!ubx!# RETURN_VALUE:"
