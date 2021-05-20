#
# This is an example makefile of how to generate a
# binary blob of a library.
#
# It is constructed to be able to be called from another build environment
# which will set up compiler tools via standard environment variables.
#
# This makescript uses
#   cc
#   objcopy
#   objdump
#   python3
# and basic shell utilities.
#

# Following parameters will be added to the library blob
# The name of the library
NAME ?= undefined
# Library version (uint32_t)
LIB_VERSION ?= 1
# Library flags (uint32_t, see lib_common/api/u_lib.h:U_LIB_HDR_FLAG_*)
LIB_FLAGS ?= 0
# Library include directories
INCLUDE ?= .
# Library source files
CFILES ?=
# Target files prefix path
PREFIX ?= .
# Path to ubxlib
UBXLIB_PATH ?= ../../

USE_ENCRYPTION ?= 0

ENC_TAG:= _enc

ifeq (1,$(USE_ENCRYPTION))
    enc_tag:= $(ENC_TAG)
    LIB_FLAGS:= 1
else
    LIB_FLAGS:= 0
    enc_tag:=
endif

CFLAGS_EXTRA = -nostartfiles -fPIC -nostdlib -MD -DUSE_ENCRYPTION=$(USE_ENCRYPTION)
LDFLAGS_EXTRA = -nostdlib
LIB_COMMON_PATH = $(UBXLIB_PATH)/common/lib_common
LIB_NAME ?= lib$(NAME)
INCLUDE += $(UBXLIB_PATH)/common/error/api  $(UBXLIB_PATH)/port/api $(LIB_COMMON_PATH)/api

OBJCOPY ?= objcopy
OBJDUMP ?= objdump
CAT     ?= cat 

VERBOSE ?= @

MKDIR_P ?= mkdir -p
SHELL_DELIMETER ?=
SHELL_COMMAND ?=

v = $(VERBOSE)
build = $(PREFIX)/build$(enc_tag)
_ldflags := $(LDFLAGS_EXTRA:-%=-Wl,-%)

objfiles := $(CFILES:%.c=$(build)/%.o)
includes := $(foreach dir,$(INCLUDE),-I$(dir))

library_c := $(PREFIX)/$(LIB_NAME)_blob$(enc_tag).c
library_bin := $(PREFIX)/$(LIB_NAME)_blob$(enc_tag).bin

library_descr_bin := $(build)/$(LIB_NAME)_descr.bin
library_code_bin := $(build)/$(LIB_NAME)_code.bin
library_code_final_bin := $(build)/$(LIB_NAME)_code_final.bin
library_code_sym := $(build)/$(LIB_NAME)_code.sym

library_code_elf := $(build)/$(LIB_NAME)_code.elf
library_descr_elf := $(build)/$(LIB_NAME)_descr.elf

library_descr_o := $(build)/descr.o
library_descr_c := $(build)/descr.c

all: $(library_c)

$(library_c): $(library_bin)
	@echo "* Generating C array"
	$(v)python $(LIB_COMMON_PATH)/genlibcfile.py $(library_bin) $(LIB_NAME) > $@
	@echo "$(abspath $@)"

$(library_bin): $(library_descr_bin) $(library_code_final_bin)
	@echo "* Building library blob"
	$(v)$(SHELL_COMMAND) $(SHELL_DELIMETER) $(CAT) $(library_descr_bin) $(library_code_final_bin) > $@ $(SHELL_DELIMETER)
	@echo "$(abspath $@)"

$(library_code_final_bin): $(library_descr_bin) $(library_code_bin) 
	@echo "* Building library blob "
	$(v)python $(LIB_COMMON_PATH)/genencryptedbin.py $(library_descr_bin) $(library_code_bin) $(USE_ENCRYPTION) $@
	@echo "$(abspath $@)"

$(library_code_bin): $(library_code_elf) 
	@echo "* Dumping library code blob"
	$(v)$(OBJCOPY) -O binary -j .text -j .rodata $< $@

$(library_code_elf): $(objfiles) 
	@echo "* Linking library"
	$(v)$(CC) -shared -o $@ $(CFLAGS) $(CFLAGS_EXTRA) $(includes) $(_ldflags) $<

$(objfiles): $(build)/%.o:%.c
	@echo "* Compiling library: $<"
	$(v)$(SHELL_COMMAND) $(SHELL_DELIMETER) $(MKDIR_P) $(@D) $(SHELL_DELIMETER)
	$(v)$(CC) $(CFLAGS) $(CFLAGS_EXTRA) $(includes) -c -o $@ $<

$(library_descr_bin): $(library_descr_elf) 
	@echo "* Dumping library descriptor blob"
	$(v)$(OBJCOPY) -O binary -j .ulibhdr -j .ulibtbl $< $@

$(library_descr_elf): $(library_descr_o)
	@echo "* Linking library descriptor"
	$(v)$(CC) -shared -o $@ $(CFLAGS) $(CFLAGS_EXTRA) $(includes) $(_ldflags) $<

$(library_descr_o): $(library_descr_c)
	@echo "* Compiling library descriptor: $<"
	$(v)$(SHELL_COMMAND) $(SHELL_DELIMETER) $(MKDIR_P) $(@D) $(SHELL_DELIMETER)
	$(v)$(CC) $(CFLAGS) $(CFLAGS_EXTRA) $(includes) -c -o $@ $<

$(library_descr_c): $(library_code_sym)
	@echo "* Generating library descriptor"
	$(v)python $(LIB_COMMON_PATH)/genlibhdr.py $< > $@

$(library_code_sym): $(library_code_elf) $(library_code_bin)
	@echo "* Dumping library symbols"
	$(v)$(OBJDUMP) -tj .text $< > $@
	$(v)echo name = $(NAME) >> $@
	$(v)echo version = $(LIB_VERSION) >> $@
	$(v)echo flags = $(LIB_FLAGS) >> $@
	$(v)echo length = $(shell $(SHELL_COMMAND) $(SHELL_DELIMETER) stat -L -c %s $(library_code_bin) $(SHELL_DELIMETER)) >> $@

clean:
	@echo "* Cleaning"
	$(v)rm -rf $(build)
	$(v)rm -rf $(build)$(ENC_TAG)
	$(v)rm -rf $(PREFIX)/$(LIB_NAME)_blob*
