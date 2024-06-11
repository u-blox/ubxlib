/*
 * Copyright 2019-2024 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** @file
 * @brief Implementation of the port I2C API for the ESP-IDF platform.
 */

#include "stddef.h"
#include "stdint.h"
#include "stdbool.h"
#include "string.h" // memset()

#include "u_cfg_sw.h"
#include "u_compiler.h" // U_ATOMIC_XXX() macros

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_i2c.h"

#include "esp_idf_version.h"

#if defined(ESP_IDF_I2C_NEW_API_DISABLE) || (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 4, 0))
# include "driver/i2c.h"
#else
# include "driver/i2c_master.h"
# include "esp_private/i2c_platform.h" // For i2c_master_get_bus_handle()
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_I2C_MAX_NUM
/** The number of I2C HW blocks that are available on ESP32.
 */
# define U_PORT_I2C_MAX_NUM 2
#endif

#if !defined(ESP_IDF_I2C_NEW_API_DISABLE) && (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 4, 0))
/** If we are using ESP-IDF older than version 5.4.0, use the old I2C API.
 */
# define ESP_IDF_I2C_NEW_API_DISABLE
#endif

/** Make a 7-bit address with a read bit.
 */
#define U_PORT_7BIT_ADDRESS_READ(__ADDRESS__) (((__ADDRESS__) << 1) | I2C_MASTER_READ)

/** Make a 7-bit address with a write bit.
 */
#define U_PORT_7BIT_ADDRESS_WRITE(__ADDRESS__) (((__ADDRESS__) << 1) | I2C_MASTER_WRITE)

/** Create a header to indicate 10-bit address transmission with a read bit.
 */
#define U_PORT_10BIT_HEADER_READ(__ADDRESS__) ((((__ADDRESS__) & (0x0300)) >> 7) | 0xF0 | I2C_MASTER_READ)

/** Create a header to indicate 10-bit address transmission with a write bit.
 */
#define U_PORT_10BIT_HEADER_WRITE(__ADDRESS__) ((((__ADDRESS__) & (0x0300)) >> 7) | 0xF0 | I2C_MASTER_WRITE)

/** Get the portion of a 10 bit address that will be sent first (which
 * is the same whether reading or writing).
 */
#define U_PORT_10BIT_ADDRESS(__ADDRESS__) ((__ADDRESS__) & 0xFF)

#ifndef U_PORT_I2C_CLOCK_SOURCE
/** The new ESP-IDF I2C API allows a clock source setting in
 * all cases, not just the ESP32X3 case.  Here we just pick
 * up what a user has defined for X3 (for backwards compatibility)
 * or otherwise the default.
 */
# ifdef U_PORT_I2C_ESP32X3_CLOCK_SOURCE
#  define U_PORT_I2C_CLOCK_SOURCE U_PORT_I2C_ESP32X3_CLOCK_SOURCE
# else
#  ifdef ESP_IDF_I2C_NEW_API_DISABLE
#   define U_PORT_I2C_CLOCK_SOURCE 0
#  else
#   define U_PORT_I2C_CLOCK_SOURCE I2C_CLK_SRC_DEFAULT
#  endif
# endif
#endif

#ifndef U_PORT_I2C_ESP32X3_CLOCK_SOURCE
/** For ESP32 and the old I2C API the I2C clock source is the
 * APB clock (80 MHz) and this code doesn't care, however for
 * ESP32x3 the clock source can be selected between the
 * crystal/XTAL (40 MHz) and the RC network which drives the
 * RTC (17.5 MHz); the I2C timeout value is calculated differently
 * depending on which source is employed.  The crystal is the
 * default: switch to the RC network by setting this #define to
 * I2C_SCLK_SRC_FLAG_LIGHT_SLEEP.
 */
# ifdef ESP_IDF_I2C_NEW_API_DISABLE
#  define U_PORT_I2C_ESP32X3_CLOCK_SOURCE 0
# else
#  define U_PORT_I2C_ESP32X3_CLOCK_SOURCE I2C_CLK_SRC_DEFAULT
# endif
#endif

#if U_PORT_I2C_ESP32X3_CLOCK_SOURCE == I2C_SCLK_SRC_FLAG_LIGHT_SLEEP
# define U_PORT_I2C_CLOCK_PERIOD_NS 57
#else
# define U_PORT_I2C_CLOCK_PERIOD_NS 25
#endif

/** The maximum value that an ESP32X3 I2C timeout
 * register can take.
 */
#define U_PORT_I2C_ESP32X3_TIMEOUT_REGISTER_MAX 22

#ifndef U_PORT_I2C_INTERRUPT_PRIORITY
/** Interrupt priority, used by the new ESP-IDF I2C API, zero
 * lets the driver select the interrupt priority.
 */
# define U_PORT_I2C_INTERRUPT_PRIORITY 0
#endif

#ifndef U_PORT_I2C_GLITCH_IGNORE_COUNT
/** Count for filtering-out I2C bus glitches in units of the APB
 * clock, used by the new ESP-IDF I2C API, recommended default
 * value from ESP-IDF documentation is 7.
 */
# define U_PORT_I2C_GLITCH_IGNORE_COUNT 7
#endif

#ifndef U_PORT_CLOCK_WAIT_TIME_MICROSECONDS
/** The clock wait time in microseconds for the new ESP-IDF
 * I2C API; this needs to take into account the amount of time
 * that an attached [GNSS] device could stretch the clock for.
 */
# define U_PORT_CLOCK_WAIT_TIME_MICROSECONDS ((U_PORT_I2C_TIMEOUT_MILLISECONDS) * 1000)
#endif

#ifndef U_PORT_I2C_TRANSACTION_TIMEOUT_MS
/** The new ESP-IDF I2C API has a transaction timeout but
 * -1 can be used to ignore it.
 */
# define U_PORT_I2C_TRANSACTION_TIMEOUT_MS -1
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure of the things we need to keep track of per I2C instance.
 */
typedef struct {
    int32_t pinSda;
    int32_t pinSdc;
    // This also used as a flag to indicate "in use",
    // even in the case of the new ESP-IDF I2C
    // API, where it would be more natural to use
    // busHandle; keeps the code simple
    int32_t clockHertz;
    bool adopted;
#ifndef ESP_IDF_I2C_NEW_API_DISABLE
    i2c_master_bus_handle_t busHandle; // Only used by the new ESP-IDF I2C API
    i2c_master_dev_handle_t devHandle;  // Only used by the new ESP-IDF I2C API
#endif
    uint16_t address;  // Only used by the new ESP-IDF I2C API
} uPortI2cData_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to ensure thread-safety.
 */
static uPortMutexHandle_t gMutex = NULL;

/** I2C device data.
 */
static uPortI2cData_t gI2cData[U_PORT_I2C_MAX_NUM];

/** Variable to keep track of the number of I2C interfaces open.
 */
static volatile int32_t gResourceAllocCount = 0;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: OLD ESP-IDF I2C API ONLY
 * -------------------------------------------------------------- */

#ifdef ESP_IDF_I2C_NEW_API_DISABLE

// Convert a millisecond timeout to a value that can be passed to
// i2c_set_timeout(); only used by the old ESP-IDF I2C API.
static int32_t timeoutMsToEsp32(int32_t timeoutMs)
{
    int32_t timeoutEsp32 = -1;

# ifdef CONFIG_IDF_TARGET_ESP32
    // Not the X3 case, good 'ole ESP32, nice and simple, units
    // of one cycle of the 80 MHz APB clock.
    timeoutEsp32 = timeoutMs * 80000;
# else
    int32_t y;
    // On ESP32X3 and similar the timeout is a power of two times
    // the chosen source clock period, so 2^x * U_PORT_I2C_CLOCK_PERIOD_NS;
    // if the 40 MHz crystal is chosen as SCLK then you have
    // 2^x * 25 ns, where x can be a maximum value of 22, so the
    // largest timeout value is 2^22 * 25ns = 104.9ms.
    for (size_t x = 0; (x < U_PORT_I2C_ESP32X3_TIMEOUT_REGISTER_MAX) &&
         (timeoutEsp32 < 0); x++) {
        y = (1UL << x) * U_PORT_I2C_CLOCK_PERIOD_NS / 1000000;
        if (y >= timeoutMs) {
            timeoutEsp32 = x;
        }
    }
# endif

    return timeoutEsp32;
}

// Convert a value returned by i2c_get_timeout() into milliseconds;
// only used by the old ESP-IDF I2C API.
static int32_t timeoutEsp32ToMs(int32_t timeoutEsp32)
{
    int32_t timeoutMs = -1;

# ifdef CONFIG_IDF_TARGET_ESP32
    // Not the X3 case, good 'ole ESP32.
    timeoutMs = timeoutEsp32 / 80000;
# else
    timeoutMs = (1UL << timeoutEsp32) * U_PORT_I2C_CLOCK_PERIOD_NS / 1000000;
# endif

    return timeoutMs;
}

// Send an I2C message, returning zero on success else negative error code.
static int32_t send(int32_t handle, uint16_t address,
                    const char *pData, size_t size, bool noStop)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    if (i2c_master_start(cmd) == ESP_OK) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        // First set up the address
        if (address > 127) {
            if ((i2c_master_write_byte(cmd, U_PORT_10BIT_HEADER_WRITE(address), true) != ESP_OK) ||
                (i2c_master_write_byte(cmd, U_PORT_10BIT_ADDRESS(address), true) != ESP_OK)) {
                errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
            }
        } else {
            if (i2c_master_write_byte(cmd, U_PORT_7BIT_ADDRESS_WRITE(address), true) != ESP_OK) {
                errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
            }
        }
        if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
            errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
            // Now add the data, with optional stop marker, and execute it
            if (((pData == NULL) || (i2c_master_write(cmd, (const uint8_t *) pData, size, true) == ESP_OK)) &&
                (noStop || (i2c_master_stop(cmd) == ESP_OK)) &&
                (i2c_master_cmd_begin(handle, cmd, (TickType_t) portMAX_DELAY) == ESP_OK)) {
                errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            }
        }
        i2c_cmd_link_delete(cmd);
    }

    return errorCode;
}

// Receive an I2C message, returning number of bytes received on success else
// negative error code.
static int32_t receive(int32_t handle, uint16_t address, char *pData, size_t size)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_PLATFORM;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    if (i2c_master_start(cmd) == ESP_OK) {
        errorCodeOrLength = (int32_t) U_ERROR_COMMON_SUCCESS;
        // First set up the address
        if (address > 127) {
            if ((i2c_master_write_byte(cmd, U_PORT_10BIT_HEADER_WRITE(address), true) != ESP_OK) ||
                (i2c_master_write_byte(cmd, U_PORT_10BIT_ADDRESS(address), true) != ESP_OK) ||
                (i2c_master_start(cmd) != ESP_OK) ||
                (i2c_master_write_byte(cmd, U_PORT_10BIT_HEADER_READ(address), true) != ESP_OK)) {
                errorCodeOrLength = (int32_t) U_ERROR_COMMON_PLATFORM;
            }
        } else {
            if (i2c_master_write_byte(cmd, U_PORT_7BIT_ADDRESS_READ(address), true) != ESP_OK) {
                errorCodeOrLength = (int32_t) U_ERROR_COMMON_PLATFORM;
            }
        }
        if (errorCodeOrLength == (int32_t) U_ERROR_COMMON_SUCCESS) {
            // Now read the data, the last byte with a nack, and execute it
            if (size > 1) {
                if ((i2c_master_read(cmd, (uint8_t *) pData, size - 1, I2C_MASTER_ACK) != ESP_OK) ||
                    (i2c_master_read_byte(cmd, (uint8_t *) (pData + size - 1), I2C_MASTER_LAST_NACK) != ESP_OK)) {
                    errorCodeOrLength = (int32_t) U_ERROR_COMMON_PLATFORM;
                }
            } else if (size > 0) {
                if (i2c_master_read_byte(cmd, (uint8_t *) (pData + size - 1), I2C_MASTER_LAST_NACK) != ESP_OK) {
                    errorCodeOrLength = (int32_t) U_ERROR_COMMON_PLATFORM;
                }
            }
            if ((errorCodeOrLength == (int32_t) U_ERROR_COMMON_SUCCESS) &&
                (i2c_master_stop(cmd) == ESP_OK) &&
                (i2c_master_cmd_begin(handle, cmd, (TickType_t) portMAX_DELAY) == ESP_OK)) {
                errorCodeOrLength = (int32_t) size;
            }
        }
        i2c_cmd_link_delete(cmd);
    }

    return errorCodeOrLength;
}

#endif // #ifdef ESP_IDF_I2C_NEW_API_DISABLE

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: NEW ESP-IDF I2C API ONLY
 * -------------------------------------------------------------- */

#ifndef ESP_IDF_I2C_NEW_API_DISABLE

// Remove any existing device for the given I2C instance; only used
// by the new ESP-IDF I2C API.
static void removeDevice(int32_t handle)
{
    if (gI2cData[handle].devHandle != NULL) {
        i2c_master_bus_rm_device(gI2cData[handle].devHandle);
        gI2cData[handle].devHandle = NULL;
        gI2cData[handle].address = 0;
    }
}

// Ensure that there is a device configuration; only used
// by the new ESP-IDF I2C API.
static int32_t ensureDevice(int32_t handle, uint16_t address,
                            int32_t clockHertz)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    i2c_device_config_t devCfg = {0};

    if (gI2cData[handle].busHandle != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        if ((gI2cData[handle].devHandle != NULL) &&
            (gI2cData[handle].address != address)) {
            // If we already have a device set up with
            // a different address, remove it
            removeDevice(handle);
        }
        if (gI2cData[handle].devHandle == NULL) {
            devCfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
            if (address > 127) {
#if SOC_I2C_SUPPORT_10BIT_ADDR
                devCfg.dev_addr_length = I2C_ADDR_BIT_LEN_10;
#else
                errorCode = (int32_t) U_ERROR_COMMON_INVALID_ADDRESS;
#endif
            }
            if (errorCode == (int32_t) U_ERROR_COMMON_SUCCESS) {
                errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                devCfg.device_address = address;
                devCfg.scl_speed_hz = clockHertz;
                devCfg.scl_wait_us = U_PORT_CLOCK_WAIT_TIME_MICROSECONDS;
                if (i2c_master_bus_add_device(gI2cData[handle].busHandle,
                                              &devCfg,
                                              &(gI2cData[handle].devHandle)) == ESP_OK) {
                    gI2cData[handle].address = devCfg.device_address;
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
        }
    }

    return errorCode;
}

#endif // #ifndef ESP_IDF_I2C_NEW_API_DISABLE

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS: MISC
 * -------------------------------------------------------------- */

// Close an I2C instance.
static void closeI2c(int32_t index)
{
    if (gI2cData[index].clockHertz > 0) {
        if (!gI2cData[index].adopted) {
#ifdef ESP_IDF_I2C_NEW_API_DISABLE
            i2c_driver_delete(index);
#else
            removeDevice(index);
            i2c_del_master_bus(gI2cData[index].busHandle);
            gI2cData[index].busHandle = NULL;
#endif
        }
        gI2cData[index].clockHertz = -1;
        U_ATOMIC_DECREMENT(&gResourceAllocCount);
    }
}

// Open an I2C instance; unlike the other static functions
// this does all the mutex locking etc.
static int32_t openI2c(int32_t i2c, int32_t pinSda, int32_t pinSdc,
                       bool controller, bool adopt)
{
    int32_t handleOrErrorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
#ifdef ESP_IDF_I2C_NEW_API_DISABLE
    i2c_config_t cfg = {0};
#else
    i2c_master_bus_config_t busCfg = {0};
#endif

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        handleOrErrorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((i2c >= 0) && (i2c < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[i2c].clockHertz < 0) && controller &&
            (adopt || ((pinSda >= 0) && (pinSdc >= 0)))) {
            handleOrErrorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
#ifdef ESP_IDF_I2C_NEW_API_DISABLE
            cfg.mode = I2C_MODE_MASTER;
            cfg.sda_io_num = pinSda;
            cfg.scl_io_num = pinSdc;
            cfg.sda_pullup_en = true;
            cfg.scl_pullup_en = true;
            cfg.master.clk_speed = U_PORT_I2C_CLOCK_FREQUENCY_HERTZ;
            cfg.clk_flags = U_PORT_I2C_ESP32X3_CLOCK_SOURCE;
            if (adopt ||
                ((i2c_param_config(i2c, &cfg) == ESP_OK) &&
                 (i2c_set_timeout(i2c, timeoutMsToEsp32(U_PORT_I2C_TIMEOUT_MILLISECONDS)) == ESP_OK) &&
                 (i2c_driver_install(i2c, I2C_MODE_MASTER, 0, 0, 0) == ESP_OK))) {
#else
            busCfg.i2c_port = i2c;
            busCfg.sda_io_num = pinSda;
            busCfg.scl_io_num = pinSdc;
            busCfg.clk_source = U_PORT_I2C_CLOCK_SOURCE;
            busCfg.intr_priority = U_PORT_I2C_INTERRUPT_PRIORITY;
            busCfg.glitch_ignore_cnt = U_PORT_I2C_GLITCH_IGNORE_COUNT;
            busCfg.flags.enable_internal_pullup = true;
            if ((adopt && (i2c_master_get_bus_handle(i2c, &(gI2cData[i2c].busHandle)) == ESP_OK)) ||
                (i2c_new_master_bus(&busCfg, &(gI2cData[i2c].busHandle)) == ESP_OK)) {
#endif
                // We need to remember the configuration in this case
                // as the only way to change the clock is to reconfigure
                // the instance entirely
                gI2cData[i2c].pinSda = pinSda;
                gI2cData[i2c].pinSdc = pinSdc;
                // In the new ESP-IDF I2C API the clock frequency is a
                // property of the device rather than the bus, which is
                // very sensible, however for convenience the code here
                // continues to use it as a "present" flag, so set it
                // in the local configuration structure
                gI2cData[i2c].clockHertz = U_PORT_I2C_CLOCK_FREQUENCY_HERTZ;
                gI2cData[i2c].adopted = adopt;
                U_ATOMIC_INCREMENT(&gResourceAllocCount);
                // Return the I2C HW block number as the handle
                handleOrErrorCode = i2c;
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return handleOrErrorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise I2C handling.
int32_t uPortI2cInit()
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCode = uPortMutexCreate(&gMutex);
        if (errorCode == 0) {
            for (size_t x = 0; x < sizeof(gI2cData) / sizeof(gI2cData[0]); x++) {
                memset(&(gI2cData[x]), 0, sizeof(gI2cData[x]));
                gI2cData[x].pinSda = -1;
                gI2cData[x].pinSdc = -1;
                gI2cData[x].clockHertz = -1;
            }
        }
    }

    return errorCode;
}

// Shutdown I2C handling.
void uPortI2cDeinit()
{
    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        // Shut down any open instances
        for (size_t x = 0; x < sizeof(gI2cData) / sizeof(gI2cData[0]); x++) {
            closeI2c(x);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

// Open an I2C instance.
int32_t uPortI2cOpen(int32_t i2c, int32_t pinSda, int32_t pinSdc,
                     bool controller)
{
    return openI2c(i2c, pinSda, pinSdc, controller, false);
}

// Adopt an I2C instance.
int32_t uPortI2cAdopt(int32_t i2c, bool controller)
{
    return openI2c(i2c, -1, -1, controller, true);
}

// Close an I2C instance.
void uPortI2cClose(int32_t handle)
{
    if ((gMutex != NULL) && (handle >= 0) &&
        (handle < sizeof(gI2cData) / sizeof(gI2cData[0]))) {

        U_PORT_MUTEX_LOCK(gMutex);

        closeI2c(handle);

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// Close an I2C instance and attempt to recover the I2C bus.
int32_t uPortI2cCloseRecoverBus(int32_t handle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].clockHertz > 0)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (!gI2cData[handle].adopted) {
                closeI2c(handle);
                // Nothing to do - bus recovery is done as required
                // on ESP-IDF; return "not supported" to indicate this
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Set the I2C clock frequency.
int32_t uPortI2cSetClock(int32_t handle, int32_t clockHertz)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
#ifdef ESP_IDF_I2C_NEW_API_DISABLE
    i2c_config_t cfg = {0};
    int32_t timeoutEsp32;
    esp_err_t x = ESP_OK;
#endif

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].clockHertz > 0) && (clockHertz > 0)) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
#ifdef ESP_IDF_I2C_NEW_API_DISABLE
            if (!gI2cData[handle].adopted) {
                errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                // In the old ESP-IDF I2C API the only way to
                // configure the clock is to do a full
                // reconfiguration of the instance
                x = i2c_get_timeout(handle, (int *) &timeoutEsp32);
                if (x == ESP_OK) {
                    cfg.mode = I2C_MODE_MASTER;
                    cfg.sda_io_num = gI2cData[handle].pinSda;
                    cfg.scl_io_num = gI2cData[handle].pinSdc;
                    cfg.sda_pullup_en = true;
                    cfg.scl_pullup_en = true;
                    cfg.master.clk_speed = clockHertz;
                    cfg.clk_flags = U_PORT_I2C_ESP32X3_CLOCK_SOURCE;

                    if (i2c_driver_delete(handle) == ESP_OK) {
                        // Mark the instance as not in use in case reconfiguring
                        // it doesn't work
                        gI2cData[handle].clockHertz = -1;
                        if ((i2c_param_config(handle, &cfg) == ESP_OK) &&
                            (i2c_set_timeout(handle, timeoutEsp32) == ESP_OK) &&
                            (i2c_driver_install(handle, I2C_MODE_MASTER, 0, 0, 0) == ESP_OK)) {
                            // All is good
                            gI2cData[handle].pinSda = cfg.sda_io_num;
                            gI2cData[handle].clockHertz = clockHertz;
                            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                        }
                    }
                }
            }
#else
            // In the new ESP-IDF I2C API the clock frequency
            // is a property of the device; it is entirely
            // under our control and does not affect the bus
            // configuration at all
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            // Release any existing device configuration
            // so that we can try setting up a device
            // with the new selected clock frequency
            removeDevice(handle);
            if (gI2cData[handle].devHandle == NULL) {
                // Perform a device configuration here, with
                // any old device address (since we don't know
                // what the right one is), ONLY so that we can
                // confirm that the platform is happy with the
                // clock speed
                errorCode = ensureDevice(handle, 1, clockHertz);
                if (errorCode == 0) {
                    // Set the clock frequency in the main configuration structure
                    gI2cData[handle].clockHertz = clockHertz;
                }
                // Now remove the device again; the send/receive
                // functions will set it up with the correct
                // device address when they need it
                removeDevice(handle);
            }
#endif
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
}

// Get the I2C clock frequency.
int32_t uPortI2cGetClock(int32_t handle)
{
    int32_t errorCodeOrClock = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCodeOrClock = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].clockHertz > 0)) {
            errorCodeOrClock = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
#ifdef ESP_IDF_I2C_NEW_API_DISABLE
            if (!gI2cData[handle].adopted) {
                errorCodeOrClock = gI2cData[handle].clockHertz;
            }
#else
            // With the new ESP-IDf I2C API the clock is always
            // under our control, even in the adopted case
            errorCodeOrClock = gI2cData[handle].clockHertz;
#endif
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCodeOrClock;
}

// Set the timeout for I2C.
int32_t uPortI2cSetTimeout(int32_t handle, int32_t timeoutMs)
{
#ifdef ESP_IDF_I2C_NEW_API_DISABLE
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].clockHertz > 0) && (timeoutMs > 0)) {
            if (!gI2cData[handle].adopted) {
                errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                if (i2c_set_timeout(handle, timeoutMsToEsp32(timeoutMs)) == ESP_OK) {
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
    return errorCode;
#else
    // There is no way to set the byte-level timeout
    // in the new ESP-IDF I2C API
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
#endif
}

// Get the timeout for I2C.
int32_t uPortI2cGetTimeout(int32_t handle)
{
#ifdef ESP_IDF_I2C_NEW_API_DISABLE
    int32_t errorCodeOrTimeout = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    int32_t timeoutEsp32;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCodeOrTimeout = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].clockHertz > 0)) {
            errorCodeOrTimeout = (int32_t) U_ERROR_COMMON_PLATFORM;
            if (i2c_get_timeout(handle, (int *) &timeoutEsp32) == ESP_OK) {
                errorCodeOrTimeout = timeoutEsp32ToMs(timeoutEsp32);
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCodeOrTimeout;
#else
    // There is no way to get the byte-level timeout
    // in the new ESP-IDF I2C API
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
#endif
}

// Send and/or receive over the I2C interface as a controller.
int32_t uPortI2cControllerExchange(int32_t handle, uint16_t address,
                                   const char *pSend, size_t bytesToSend,
                                   char *pReceive, size_t bytesToReceive,
                                   bool noInterveningStop)
{
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].clockHertz > 0) &&
            ((pSend != NULL) || (bytesToSend == 0)) &&
            ((pReceive != NULL) || (bytesToReceive == 0))) {
            errorCodeOrLength = (int32_t) U_ERROR_COMMON_SUCCESS;
#ifdef ESP_IDF_I2C_NEW_API_DISABLE
            if (pSend != NULL) {
                errorCodeOrLength = send(handle, address, pSend, bytesToSend,
                                         noInterveningStop);
            }
            if ((errorCodeOrLength == (int32_t) U_ERROR_COMMON_SUCCESS) &&
                (pReceive != NULL)) {
                errorCodeOrLength = receive(handle, address, pReceive, bytesToReceive);
            }
#else
            // With the new ESP-IDF I2C API an explicit "send and receive" function
            // must be called for the no-intervening-stop case, hence we can't have
            // separate send/receive sub-functions, gotta do both at once; since the
            // new API is a lot simpler we can just call the functions here
            // First, make sure we have a device set up
            errorCodeOrLength = ensureDevice(handle, address,
                                             gI2cData[handle].clockHertz);
            if (errorCodeOrLength == 0) {
                errorCodeOrLength = (int32_t) U_ERROR_COMMON_PLATFORM;
                if (pSend != NULL) {
                    if (noInterveningStop) {
                        // This will do a transmit, no stop bit, then a receive
                        if (i2c_master_transmit_receive(gI2cData[handle].devHandle,
                                                        (const uint8_t *) pSend,
                                                        bytesToSend,
                                                        (uint8_t *) pReceive,
                                                        bytesToReceive,
                                                        U_PORT_I2C_TRANSACTION_TIMEOUT_MS) == ESP_OK) {
                            errorCodeOrLength = (int32_t) bytesToReceive;
                            // Done with the receive here
                            pReceive = NULL;
                        }
                    } else {
                        if (i2c_master_transmit(gI2cData[handle].devHandle,
                                                (const uint8_t *) pSend,
                                                bytesToSend,
                                                U_PORT_I2C_TRANSACTION_TIMEOUT_MS) == ESP_OK) {
                            errorCodeOrLength = (int32_t) U_ERROR_COMMON_SUCCESS;
                        }
                    }
                }
                if (((pSend == NULL) || (errorCodeOrLength >= 0)) &&
                    (pReceive != NULL)) {
                    if (i2c_master_receive(gI2cData[handle].devHandle,
                                           (uint8_t *) pReceive,
                                           bytesToReceive,
                                           U_PORT_I2C_TRANSACTION_TIMEOUT_MS) == ESP_OK) {
                        errorCodeOrLength = (int32_t) bytesToReceive;
                    }
                }
                if ((pReceive == NULL) && (pSend == NULL)) {
                    // Send a probe; the error return codes here are
                    // quite specific so we can also be quite specific
                    esp_err_t x = i2c_master_probe(gI2cData[handle].busHandle,
                                                   address,
                                                   U_PORT_I2C_TRANSACTION_TIMEOUT_MS);
                    switch (x) {
                        case ESP_OK:
                            errorCodeOrLength = (int32_t) U_ERROR_COMMON_SUCCESS;
                            break;
                        case ESP_ERR_TIMEOUT:
                            errorCodeOrLength = (int32_t) U_ERROR_COMMON_TIMEOUT;
                            break;
                        default:
                            errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_FOUND;
                            break;
                    }
                }
                if (errorCodeOrLength < 0) {
                    // If the device has sent a NACK the I2C bus seems
                    // to get stuck; give it a kick
                    i2c_master_bus_reset(gI2cData[handle].busHandle);
                }
            }
#endif
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCodeOrLength;
}

/** \deprecated please use uPortI2cControllerExchange() instead. */
// Send and/or receive over the I2C interface as a controller.
int32_t uPortI2cControllerSendReceive(int32_t handle, uint16_t address,
                                      const char *pSend, size_t bytesToSend,
                                      char *pReceive, size_t bytesToReceive)
{
#ifdef ESP_IDF_I2C_NEW_API_DISABLE
    int32_t errorCodeOrLength = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCodeOrLength = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].clockHertz > 0) &&
            ((pSend != NULL) || (bytesToSend == 0)) &&
            ((pReceive != NULL) || (bytesToReceive == 0))) {
            errorCodeOrLength = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pSend != NULL) {
                errorCodeOrLength = send(handle, address, pSend, bytesToSend, false);
            }
            if ((errorCodeOrLength == (int32_t) U_ERROR_COMMON_SUCCESS) &&
                (pReceive != NULL)) {
                errorCodeOrLength = receive(handle, address, pReceive, bytesToReceive);
            }
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCodeOrLength;
#else
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
#endif
}

/** \deprecated please use uPortI2cControllerExchange() instead. */
// Perform a send over the I2C interface as a controller.
int32_t uPortI2cControllerSend(int32_t handle, uint16_t address,
                               const char *pSend, size_t bytesToSend,
                               bool noStop)
{
#ifdef ESP_IDF_I2C_NEW_API_DISABLE
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;

    if (gMutex != NULL) {

        U_PORT_MUTEX_LOCK(gMutex);

        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if ((handle >= 0) && (handle < sizeof(gI2cData) / sizeof(gI2cData[0])) &&
            (gI2cData[handle].clockHertz > 0) &&
            ((pSend != NULL) || (bytesToSend == 0))) {
            errorCode = send(handle, address, pSend, bytesToSend, noStop);
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }

    return errorCode;
#else
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
#endif
}

// Get the number of I2C interfaces currently open.
int32_t uPortI2cResourceAllocCount()
{
    return U_ATOMIC_GET(&gResourceAllocCount);
}

// End of file
