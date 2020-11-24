# Define the fibonacci library
# Following parameters will be added to the library blob

# Library name
NAME = fibonacci
# Library version (uint32_t)
LIB_VERSION = 1
# Library flags (uint32_t, see lib_common/api/u_lib.h:U_LIB_HDR_FLAG_*)
LIB_FLAGS = 4 # this lib needs malloc

# Building parameters

# Path to ubxlib
UBXLIB_PATH = ../../../
# Library include directories
INCLUDE = . api
# Library source files
CFILES = src/lib_fibonacci.c

# call the library utility builder script
include $(UBXLIB_PATH)/common/lib_common/makelib.mk
