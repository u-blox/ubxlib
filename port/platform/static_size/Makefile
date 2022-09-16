# Note "realpath" in the line below to remove the last trailing slash for neatness
MAKEFILE_DIR := $(realpath $(subst \,/,$(dir $(lastword $(MAKEFILE_LIST)))))
UBXLIB_BASE ?= $(realpath $(MAKEFILE_DIR)/../../..)
OUTDIR ?= output

ifeq ($(OS),Windows_NT)
# Force Windows as otherise, if sh.exe is in the path, make
# will use it and mess up on slashes/brackets in file paths 
SHELL = cmd
endif

$(info MAKEFILE_DIR is ${MAKEFILE_DIR})
$(info UBXLIB_BASE is ${UBXLIB_BASE})
$(info OUTDIR is ${OUTDIR})
$(info SHELL is ${SHELL})

TARGET_FLOAT = static_size_float.elf
TARGET_NO_FLOAT = static_size_no_float.elf
OUTDIR_FLOAT := $(OUTDIR)/float
OUTDIR_NO_FLOAT := $(OUTDIR)/no_float
OBJDIR_FLOAT := $(OUTDIR_FLOAT)/obj
OBJDIR_NO_FLOAT := $(OUTDIR_NO_FLOAT)/obj

# Commands
RM = rm
CC = arm-none-eabi-gcc
SIZE = arm-none-eabi-size

ifeq ($(OS),Windows_NT)
mkdir = mkdir $(subst /,\,$(1)) > nul 2>&1 || (exit 0)
else
mkdir = mkdir -p $(1)
endif

# Comment out line below to show all command output
SILENT = @

# Compiler flags
override CFLAGS += -Os -g0
LDFLAGS += -Wl,--cref --specs=nano.specs -lc -lnosys

# Compiler flags for no float
CFLAGS_NO_FLOAT += -mcpu=cortex-m4+nofp
LDFLAGS_NO_FLOAT += -Wl,-Map=$(OUTDIR_NO_FLOAT)/static_size_no_float.map

# Compiler flags for float
CFLAGS_FLOAT += -mcpu=cortex-m4 -mfloat-abi=hard -mfpu=fpv4-sp-d16
LDFLAGS_FLOAT += -Wl,-Map=$(OUTDIR_FLOAT)/static_size.map -lm

# Include ubxlib src and inc
UBXLIB_FEATURES = cell gnss short_range
# ubxlib.mk will define UBXLIB_INC, UBXLIB_PRIVATE_INC and UBXLIB_SRC for us
include $(UBXLIB_BASE)/port/ubxlib.mk

# Source files
SRCS += \
   $(UBXLIB_SRC) \
   $(UBXLIB_BASE)/port/clib/u_port_clib_mktime64.c \
   stubs/u_port_stub.c \
   stubs/u_lib_stub.c \
   stubs/u_main_stub.c

# Include directories
INC += \
   $(UBXLIB_INC) \
   $(UBXLIB_PRIVATE_INC) \
   $(UBXLIB_BASE)/cfg \
   $(UBXLIB_BASE)/port/clib \
   stubs

ABS_SRCS = $(realpath $(SRCS))
OBJS_FLOAT := $(ABS_SRCS:$(UBXLIB_BASE)/%.c=$(OBJDIR_FLOAT)/%.o)
OBJS_NO_FLOAT := $(ABS_SRCS:$(UBXLIB_BASE)/%.c=$(OBJDIR_NO_FLOAT)/%.o)

override CFLAGS += $(INC:%=-I%)

.PHONY: clean float_size no_float_size

all: float_size no_float_size

clean:
	$(RM) -rf $(OUTDIR)

# Float recepies
$(OBJDIR_FLOAT)%.o: $(UBXLIB_BASE)%.c
	$(SILENT)$(call mkdir,$(@D))
	@echo CC $<
	$(SILENT)$(CC) -c -o $@ $< $(CFLAGS) $(CFLAGS_FLOAT)

$(OUTDIR_FLOAT)/$(TARGET_FLOAT): $(OBJS_FLOAT)
	$(SILENT)$(call mkdir,$(@D))
	@echo Linking $@
	$(SILENT)$(CC) -o $@ $^ $(LDFLAGS) $(LDFLAGS_FLOAT) $(CFLAGS_FLOAT)

float_size: $(OUTDIR_FLOAT)/$(TARGET_FLOAT)
	$(SILENT)$(SIZE) -G $(OBJS_FLOAT) $(OUTDIR_FLOAT)/$(TARGET_FLOAT)

# No float recepies
$(OBJDIR_NO_FLOAT)%.o: $(UBXLIB_BASE)%.c
	$(SILENT)$(call mkdir,$(@D))
	@echo CC $<
	$(SILENT)$(CC) -c -o $@ $< $(CFLAGS) $(CFLAGS_NO_FLOAT)

$(OUTDIR_NO_FLOAT)/$(TARGET_NO_FLOAT): $(OBJS_NO_FLOAT)
	$(SILENT)$(call mkdir,$(@D))
	@echo Linking $@
	$(SILENT)$(CC) -o $@ $^ $(LDFLAGS) $(LDFLAGS_NO_FLOAT) $(CFLAGS_NO_FLOAT)

no_float_size: $(OUTDIR_NO_FLOAT)/$(TARGET_NO_FLOAT)
	$(SILENT)$(SIZE) -G $(OBJS_NO_FLOAT) $(OUTDIR_NO_FLOAT)/$(TARGET_NO_FLOAT)
