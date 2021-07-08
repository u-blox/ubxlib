/*
 * Copyright 2020 u-blox Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** @file
 * @brief Stuff private to the NRF52 porting layer.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"
#include "u_port.h"
#include "u_port_private.h"
#include "u_cfg_hw_platform_specific.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include "nrfx.h"
#include "nrfx_timer.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

// The CC channel to use for timer compares
#define U_PORT_TICK_TIMER_COMPARE_CHANNEL 0

// The CC channel to use for timer captures
#define U_PORT_TICK_TIMER_CAPTURE_CHANNEL 1

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

// The tick timer.
static nrfx_timer_t gTickTimer = NRFX_TIMER_INSTANCE(U_CFG_HW_TICK_TIMER_INSTANCE);

// Overflow counter that allows us to keep 64 bit time.
static int64_t gTickTimerOverflowCount;

// The tick timer offset, used to compensate for jumps
// required when switching to UART mode. This can
// be a 32 bit value since any offset over and
// above the overflow count will be absorbed
// into the overflow count and the overflow
// count is max U_PORT_TICK_TIMER_LIMIT_NORMAL_MODE
// of 0xFFFFFF.
static int32_t gTickTimerOffset;

// Flag to indicate whether the timer is running in
// "UART" mode or normal mode.  When it is running in
// UART mode it has to overflow quickly so that the
// callback can be used as an RX timeout.
static bool gTickTimerUartMode;

// A callback to be called when the UART overflows.
static void (*gpCb) (void *);

// The user parameter for the callback.
static void *gpCbParameter;

// Mutex to protect RTT logging.
SemaphoreHandle_t gRttLoggingMutex = NULL;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// The tick handler.
static void tickTimerHandler(nrf_timer_event_t eventType, void *pContext)
{
    (void) pContext;

    if (eventType == NRF_TIMER_EVENT_COMPARE0) {
        gTickTimerOverflowCount++;
        if (gpCb != NULL) {
            gpCb(gpCbParameter);
        }
    }
}

// Start the tick timer.
static int32_t tickTimerStart(nrfx_timer_config_t *pTimerCfg,
                              int32_t limit)
{
    int32_t errorCode = U_ERROR_COMMON_PLATFORM;

    if (nrfx_timer_init(&gTickTimer,
                        pTimerCfg,
                        tickTimerHandler) == NRFX_SUCCESS) {
        // Set the compare interrupt on CC zero comparing
        // with limit, clearing when the
        // compare is reached and enable the interrupt
        nrfx_timer_extended_compare(&gTickTimer,
                                    U_PORT_TICK_TIMER_COMPARE_CHANNEL,
                                    limit,
                                    NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK,
                                    true);

        // Clear the timer
        nrfx_timer_clear(&gTickTimer);

        // Now enable the timer
        nrfx_timer_enable(&gTickTimer);

        errorCode = U_ERROR_COMMON_SUCCESS;
    }

    return errorCode;
}

// Stop the tick timer.
static void tickTimerStop()
{
    nrfx_timer_disable(&gTickTimer);
    nrfx_timer_compare_int_disable(&gTickTimer,
                                   U_PORT_TICK_TIMER_COMPARE_CHANNEL);
    nrfx_timer_uninit(&gTickTimer);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS SPECIFIC TO THIS PORT
 * -------------------------------------------------------------- */

// Initialise logging.
void uPortPrivateLoggingInit()
{
    //Create a mutex that can be used with RTT logging
    gRttLoggingMutex = xSemaphoreCreateMutex();
}

// Lock logging.
void uPortPrivateLoggingLock()
{
    xSemaphoreTake(gRttLoggingMutex, (portTickType) portMAX_DELAY);
}

// Unlock logging.
void uPortPrivateLoggingUnlock()
{
    xSemaphoreGive(gRttLoggingMutex);
}

// Convert a tick value to a microsecond value
inline int64_t uPortPrivateTicksToUs(int32_t tickValue)
{
    // Convert to milliseconds when running at 31.25 kHz, one tick
    // every 32 us, so shift left 5.
    return ((int64_t) tickValue) << 5;
}

// Initalise the private stuff.
int32_t uPortPrivateInit()
{
    nrfx_timer_config_t timerCfg = NRFX_TIMER_DEFAULT_CONFIG;

    gTickTimerOverflowCount = 0;
    gTickTimerOffset = 0;
    gTickTimerUartMode = false;
    gpCb = NULL;
    gpCbParameter = NULL;
    timerCfg.frequency = U_PORT_TICK_TIMER_FREQUENCY_HZ;
    timerCfg.bit_width = U_PORT_TICK_TIMER_BIT_WIDTH;

    return tickTimerStart(&timerCfg,
                          U_PORT_TICK_TIMER_LIMIT_NORMAL_MODE);
}

// Deinitialise the private stuff.
void uPortPrivateDeinit()
{
    tickTimerStop();
}

// Register a callback to be called when the tick timer
// overflow interrupt occurs.
void uPortPrivateTickTimeSetInterruptCb(void (*pCb) (void *),
                                        void *pCbParameter)
{
    gpCb = pCb;
    gpCbParameter = pCbParameter;
}

// Switch the tick timer to UART mode.
void uPortPrivateTickTimeUartMode()
{
    int32_t tickTimerValue = 0;
    int32_t x;

    if (!gTickTimerUartMode) {
        // Pause the timer
        nrfx_timer_pause(&gTickTimer);
        // Set the new compare value
        nrf_timer_cc_write(gTickTimer.p_reg,
                           U_PORT_TICK_TIMER_COMPARE_CHANNEL,
                           U_PORT_TICK_TIMER_LIMIT_UART_MODE);
        // Re-calculate the overflow count
        // for this bit-width
        gTickTimerOverflowCount <<= U_PORT_TICK_TIMER_LIMIT_DIFF;

        // It is possible that the timer is already
        // beyond the UART limit, so we reset the
        // timer here.
        // First read the current tick value and pour it into
        // gTickTimerOverflowCount and gTickTimerOffset
        tickTimerValue = nrfx_timer_capture(&gTickTimer,
                                            U_PORT_TICK_TIMER_CAPTURE_CHANNEL);
        // Transfer whatever we can of the current value into
        // the overflow count
        x = tickTimerValue / (U_PORT_TICK_TIMER_LIMIT_UART_MODE + 1);
        gTickTimerOverflowCount += x;
        tickTimerValue -= x * (U_PORT_TICK_TIMER_LIMIT_UART_MODE + 1);
        // Transfer any of the offset we can into the overflow count
        x = gTickTimerOffset / (U_PORT_TICK_TIMER_LIMIT_UART_MODE + 1);
        gTickTimerOverflowCount += x;
        gTickTimerOffset -= x * (U_PORT_TICK_TIMER_LIMIT_UART_MODE + 1);
        // Finally add the remainder of the current value into the offset
        gTickTimerOffset += tickTimerValue;
        // ...and clear the timer
        nrfx_timer_clear(&gTickTimer);

        gTickTimerUartMode = true;
        // Resume the timer
        nrfx_timer_resume(&gTickTimer);
    }
}

// Switch the tick timer back to normal mode.
void uPortPrivateTickTimeNormalMode()
{
    int64_t remainderOverflowTicks;
    int32_t x;

    if (gTickTimerUartMode) {
        // Pause the timer
        nrfx_timer_pause(&gTickTimer);
        // Set the new compare value
        nrf_timer_cc_write(gTickTimer.p_reg,
                           U_PORT_TICK_TIMER_COMPARE_CHANNEL,
                           U_PORT_TICK_TIMER_LIMIT_NORMAL_MODE);
        // No danger of the tick count being beyond the
        // limit here, can just continue counting,
        // but we need to convert the overflow count
        // into "normal time" (as opposed to "uart time")
        // units without losing anything.
        // Work out what will be left after we reduce
        // the overflow count by the ratio of the two
        // limits.
        // Remember the overflow count.
        remainderOverflowTicks = gTickTimerOverflowCount;
        // Re-calculate the overflow count
        // for this bit-width
        gTickTimerOverflowCount >>= U_PORT_TICK_TIMER_LIMIT_DIFF;
        // Work out the remainder
        remainderOverflowTicks -= gTickTimerOverflowCount <<
                                  U_PORT_TICK_TIMER_LIMIT_DIFF;
        // Convert the overflow remainder value into ticks.
        remainderOverflowTicks *= U_PORT_TICK_TIMER_LIMIT_UART_MODE + 1;
        // Put what we can of it into the overflow count
        x = remainderOverflowTicks / (U_PORT_TICK_TIMER_LIMIT_NORMAL_MODE + 1);
        gTickTimerOverflowCount += x;
        remainderOverflowTicks -= x * (U_PORT_TICK_TIMER_LIMIT_NORMAL_MODE + 1);
        // Transfer any of the offset we can into the overflow count
        x = gTickTimerOffset / (U_PORT_TICK_TIMER_LIMIT_NORMAL_MODE + 1);
        gTickTimerOverflowCount += x;
        gTickTimerOffset -= x * (U_PORT_TICK_TIMER_LIMIT_NORMAL_MODE + 1);
        // Finally add what's left of the remainder of
        // the overflow value into the offset
        gTickTimerOffset += remainderOverflowTicks;

        // Continue ticking
        gTickTimerUartMode = false;
        nrfx_timer_resume(&gTickTimer);
    }
}

// Get the current tick converted to a time in milliseconds.
int64_t uPortPrivateGetTickTimeMs()
{
    int64_t tickTimerValue = 0;

    // Read the timer
    tickTimerValue = nrfx_timer_capture(&gTickTimer,
                                        U_PORT_TICK_TIMER_CAPTURE_CHANNEL);

    // Add any offset from converting to UART mode.
    tickTimerValue += gTickTimerOffset;

    // Convert to milliseconds when running at 31.25 kHz, one tick
    // every 32 us, so shift left 5, then divide by 1000.
    tickTimerValue = (((uint64_t) tickTimerValue) << 5) / 1000;
    if (gTickTimerUartMode) {
        // The timer is 11 bits wide so each overflow represents
        // ((1 / 31250) * 2048) seconds, 65.536 milliseconds
        // or x * 65536 / 1000
        tickTimerValue += (((uint64_t) gTickTimerOverflowCount) << 16) / 1000;
    } else {
        // The timer is 24 bits wide so each overflow represents
        // ((1 / 31250) * (2 ^ 24)) seconds, about very 537 seconds.
        // Here just multiply 'cos ARM can do that in one clock cycle
        tickTimerValue += gTickTimerOverflowCount * 536871;
    }

    return tickTimerValue;
}

// End of file
