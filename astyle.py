#!/usr/bin/env python

'''Run AStyle on all code.'''

# Run this from the ubxlib root directory to run AStyle, which must be on the path,
# on all relevant files in the ubxlib directory tree, using the standard ubxlib astyle.cfg

import subprocess

with subprocess.Popen("astyle --options=astyle.cfg --suffix=none --verbose " \
                      "--errors-to-stdout --recursive \"*.c,*.h,*.cpp,*.hpp\"",
                      stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                      universal_newlines=True) as astyle:
    output = astyle.communicate()
    for line in output:
        print (line, end=" ")
