# This is a shared Makefile used for the ports using classic make build system.
# It is used for collecting source code files and include directories
# that are selected based on UBXLIB_FEATURES. Check README.md for details.

# For each entry in UBXLIB_MODULE_DIRS the following will be done:
# * Append /api and add result to UBXLIB_INC
# * Append /src and add result to UBXLIB_SRC_DIRS
# * Append /test and add result to UBXLIB_TEST_DIRS
UBXLIB_MODULE_DIRS = \
	${UBXLIB_BASE}/common/at_client \
	${UBXLIB_BASE}/common/error \
	${UBXLIB_BASE}/common/assert \
	${UBXLIB_BASE}/common/timeout \
	${UBXLIB_BASE}/common/location \
	${UBXLIB_BASE}/common/mqtt_client \
	${UBXLIB_BASE}/common/http_client \
	${UBXLIB_BASE}/common/security \
	${UBXLIB_BASE}/common/sock \
	${UBXLIB_BASE}/common/ubx_protocol \
	${UBXLIB_BASE}/common/spartn \
	${UBXLIB_BASE}/common/utils \
	${UBXLIB_BASE}/common/dns \
	${UBXLIB_BASE}/common/geofence \
	${UBXLIB_BASE}/port/platform/common/debug_utils

# Additional source directories
UBXLIB_SRC_DIRS += \
	${UBXLIB_BASE}/port/platform/common/event_queue \
	${UBXLIB_BASE}/port/platform/common/mutex_debug \
	${UBXLIB_BASE}/port/platform/common/log_ram


# Additional include directories
UBXLIB_INC += \
	${UBXLIB_BASE} \
	${UBXLIB_BASE}/cfg \
	${UBXLIB_BASE}/common/type/api \
	${UBXLIB_BASE}/port/api

UBXLIB_PRIVATE_INC += \
	${UBXLIB_BASE}/port/platform/common/event_queue \
	${UBXLIB_BASE}/port/platform/common/mutex_debug \
	${UBXLIB_BASE}/port/platform/common/debug_utils/src/freertos/additions \
	${UBXLIB_BASE}/port/platform/common/log_ram

SRC_LIST =

# Device and network require special care since they contain stub & optional files
SRC_LIST += ${UBXLIB_BASE}/common/network/src/u_network.c
SRC_LIST += ${UBXLIB_BASE}/common/network/src/u_network_shared.c
SRC_LIST += ${UBXLIB_BASE}/common/network/src/u_network_private_ble_extmod_stub.c
SRC_LIST += ${UBXLIB_BASE}/common/network/src/u_network_private_cell_stub.c
SRC_LIST += ${UBXLIB_BASE}/common/network/src/u_network_private_gnss_stub.c
SRC_LIST += ${UBXLIB_BASE}/common/network/src/u_network_private_wifi_stub.c
UBXLIB_INC += ${UBXLIB_BASE}/common/network/api
UBXLIB_PRIVATE_INC += ${UBXLIB_BASE}/common/network/src
SRC_LIST += ${UBXLIB_BASE}/common/device/src/u_device.c
SRC_LIST += ${UBXLIB_BASE}/common/device/src/u_device_serial.c
SRC_LIST += ${UBXLIB_BASE}/common/device/src/u_device_serial_wrapped.c
SRC_LIST += ${UBXLIB_BASE}/common/device/src/u_device_shared.c
SRC_LIST += ${UBXLIB_BASE}/common/device/src/u_device_private.c
SRC_LIST += ${UBXLIB_BASE}/common/device/src/u_device_private_cell_stub.c
SRC_LIST += ${UBXLIB_BASE}/common/device/src/u_device_private_gnss_stub.c
SRC_LIST += ${UBXLIB_BASE}/common/device/src/u_device_private_short_range_stub.c
UBXLIB_INC += ${UBXLIB_BASE}/common/device/api
UBXLIB_PRIVATE_INC += ${UBXLIB_BASE}/common/device/src

# Default malloc()/free() implementation
SRC_LIST += ${UBXLIB_BASE}/port/u_port_heap.c

# Default uPortGetTimezoneOffsetSeconds() implementation
SRC_LIST += ${UBXLIB_BASE}/port/u_port_timezone.c

# Default uPortXxxResource implementation
SRC_LIST += ${UBXLIB_BASE}/port/u_port_resource.c

# Default implementation for certain uPortI2cXxx() and uPortSpiXxx() functions
SRC_LIST += ${UBXLIB_BASE}/port/u_port_i2c_default.c
SRC_LIST += ${UBXLIB_BASE}/port/u_port_spi_default.c

# Default implementation for uPortNamePipeXxx()
SRC_LIST += ${UBXLIB_BASE}/port/u_port_named_pipe_default.c

# Default uPortPppAttach()/uPortPppDetach() implementation
SRC_LIST += ${UBXLIB_BASE}/port/u_port_ppp_default.c

# Default uPortDeviceXxx implementation
SRC_LIST += ${UBXLIB_BASE}/port/u_port_board_cfg.c

# Optional short range related files and directories
ifneq ($(filter short_range,$(UBXLIB_FEATURES)),)
UBXLIB_MODULE_DIRS += \
	${UBXLIB_BASE}/common/short_range \
	${UBXLIB_BASE}/ble \
	${UBXLIB_BASE}/wifi

SRC_LIST += \
	${UBXLIB_BASE}/common/network/src/u_network_private_ble_extmod.c \
	${UBXLIB_BASE}/common/network/src/u_network_private_ble_intmod.c \
	${UBXLIB_BASE}/common/network/src/u_network_private_wifi.c \
	${UBXLIB_BASE}/common/device/src/u_device_private_short_range.c
else
# Make the linker happy
SRC_LIST += \
	${UBXLIB_BASE}/common/network/src/u_network_private_ble_extmod_link.c \
	${UBXLIB_BASE}/common/network/src/u_network_private_wifi_link.c \
	${UBXLIB_BASE}/common/device/src/u_device_private_short_range_link.c
# Always add all of the includes
UBXLIB_INC += \
	${UBXLIB_BASE}/common/short_range/api \
	${UBXLIB_BASE}/ble/api \
	${UBXLIB_BASE}/wifi/api
UBXLIB_PRIVATE_INC += \
	${UBXLIB_BASE}/common/short_range/src \
	${UBXLIB_BASE}/ble/src \
	${UBXLIB_BASE}/wifi/src
UBXLIB_TEST_INC += \
	${UBXLIB_BASE}/common/short_range/test \
	${UBXLIB_BASE}/ble/test \
	${UBXLIB_BASE}/wifi/test
endif

# Optional cell related files and directories
ifneq ($(filter cell,$(UBXLIB_FEATURES)),)
UBXLIB_MODULE_DIRS += ${UBXLIB_BASE}/cell
SRC_LIST += \
	${UBXLIB_BASE}/common/network/src/u_network_private_cell.c \
	${UBXLIB_BASE}/common/device/src/u_device_private_cell.c
else
# Make the linker happy
SRC_LIST += \
	${UBXLIB_BASE}/common/network/src/u_network_private_cell_link.c \
	${UBXLIB_BASE}/common/device/src/u_device_private_cell_link.c
# Always add all of the includes
UBXLIB_INC += \
	${UBXLIB_BASE}/cell/api
UBXLIB_PRIVATE_INC += \
	${UBXLIB_BASE}/cell/src
UBXLIB_TEST_INC += \
	${UBXLIB_BASE}/cell/test
endif

# Optional GNSS related files and directories
ifneq ($(filter gnss,$(UBXLIB_FEATURES)),)
UBXLIB_MODULE_DIRS += ${UBXLIB_BASE}/gnss
SRC_LIST += \
	${UBXLIB_BASE}/common/network/src/u_network_private_gnss.c \
	${UBXLIB_BASE}/common/device/src/u_device_private_gnss.c \
	${UBXLIB_BASE}/common/geofence/src/dummy/u_geofence_geodesic.c \
	${UBXLIB_BASE}/gnss/src/lib_mga/u_lib_mga.c
else
# Make the linker happy
SRC_LIST += \
	${UBXLIB_BASE}/common/network/src/u_network_private_gnss_link.c \
	${UBXLIB_BASE}/common/device/src/u_device_private_gnss_link.c
# Always add all of the includes
UBXLIB_INC += \
	${UBXLIB_BASE}/gnss/api
UBXLIB_PRIVATE_INC += \
	${UBXLIB_BASE}/gnss/src
UBXLIB_TEST_INC += \
	${UBXLIB_BASE}/gnss/test
endif
# The lib_mga subdirectory won't be added by the UBXLIB_MODULE_DIRS, so add it explicitly here for all cases
UBXLIB_PRIVATE_INC += \
	${UBXLIB_BASE}/gnss/src/lib_mga

# lib_common
ifneq ($(filter u_lib,$(UBXLIB_FEATURES)),)
UBXLIB_INC += ${UBXLIB_BASE}/common/lib_common/api
UBXLIB_SRC_DIRS += ${UBXLIB_BASE}/common/lib_common/src
endif


# Extra test directories
UBXLIB_TEST_DIRS += \
	${UBXLIB_BASE}/port/platform/common/runner \
	${UBXLIB_BASE}/port/platform/common/test_util \
	${UBXLIB_BASE}/port/platform/common/test \
	${UBXLIB_BASE}/port/test \
	${UBXLIB_BASE}/common/device/test \
	${UBXLIB_BASE}/common/network/test
# Examples are compiled as tests
UBXLIB_TEST_DIRS += \
	${UBXLIB_BASE}/example/sockets \
	${UBXLIB_BASE}/example/security \
	${UBXLIB_BASE}/example/mqtt_client \
	${UBXLIB_BASE}/example/http_client \
	${UBXLIB_BASE}/example/location \
	${UBXLIB_BASE}/example/cell/lte_cfg \
	${UBXLIB_BASE}/example/cell/power_saving \
	${UBXLIB_BASE}/example/gnss \
	${UBXLIB_BASE}/example/utilities/c030_module_fw_update



# For each module dir:
# * Append /api and add result to UBXLIB_INC
# * Append /src and add result to UBXLIB_SRC_DIRS
# * Append /src and add result to UBXLIB_PRIVATE_INC
# * Append /test and add result to UBXLIB_TEST_DIRS
UBXLIB_INC += $(wildcard $(addsuffix /api, $(UBXLIB_MODULE_DIRS)))
UBXLIB_SRC_DIRS += $(wildcard $(addsuffix /src, $(UBXLIB_MODULE_DIRS)))
UBXLIB_PRIVATE_INC += $(wildcard $(addsuffix /src, $(UBXLIB_MODULE_DIRS)))
UBXLIB_TEST_DIRS += $(wildcard $(addsuffix /test, $(UBXLIB_MODULE_DIRS)))

# Get all .c files in each UBXLIB_SRC_DIRS and add these to UBXLIB_SRC
SRC_LIST += \
	$(foreach dir, $(UBXLIB_SRC_DIRS), \
		$(sort $(wildcard $(dir)/*.c)) \
	)

# Add all the files to UBXLIB_SRC variable.
# Check for possible gen2 replacements when feature is set.
ifneq ($(filter short_range_gen2,$(UBXLIB_FEATURES)),)
  UBXLIB_SRC += \
    $(foreach file,\
      $(SRC_LIST),\
      $(if \
        $(wildcard $(dir $(file))gen2/$(notdir $(file))),\
        $(dir $(file))gen2/$(notdir $(file)),\
        $(file)\
      )\
    )
  # Shortrange second generation AT module
  GEN2_AT_DIR = ${UBXLIB_BASE}/common/short_range/src/gen2/ucxclient
  UBXLIB_INC += ${GEN2_AT_DIR}/inc ${GEN2_AT_DIR}/ucx_api
  UBXLIB_SRC += $(sort $(wildcard ${GEN2_AT_DIR}/src/*.c))
  UBXLIB_SRC += $(sort $(wildcard ${GEN2_AT_DIR}/ucx_api/*.c))
  override CFLAGS += -DU_UCONNECT_GEN2 -DU_CX_AT_CONFIG_FILE="\"../../ucx_config.h\""
else
  UBXLIB_SRC += $(SRC_LIST)
endif

# Get all .c files in each UBXLIB_TEST_DIRS and add these to UBXLIB_TEST_SRC
UBXLIB_TEST_SRC += \
	$(foreach dir, $(UBXLIB_TEST_DIRS), \
		$(sort $(wildcard $(dir)/*.c)) \
	)
UBXLIB_TEST_INC += $(UBXLIB_TEST_DIRS)
