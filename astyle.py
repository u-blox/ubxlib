#!/usr/bin/env python

'''Run AStyle on all code.'''

# Run this from the ubxlib root directory to run AStyle, which must be on the path,
# on all relevant files in the ubxlib directory tree, using the standard ubxlib astyle.cfg

import subprocess
import sys

with subprocess.Popen(["astyle", "--options=astyle.cfg", "--suffix=none", "--verbose",
                      "--errors-to-stdout", "--recursive", "*.c,*.h,*.cpp,*.hpp"],
                      stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                      universal_newlines=True) as astyle:
    fail = False
    output, _ = astyle.communicate()
    for line in output.splitlines():
        if line.startswith("Formatted"):
            fail = True
        print (line)
    if astyle.returncode != 0:
        fail = True
    sys.exit(1 if fail else 0)
