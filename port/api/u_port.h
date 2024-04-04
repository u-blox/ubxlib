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

#ifndef _U_PORT_H_
#define _U_PORT_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_compiler.h" // U_INLINE

/** \addtogroup __port __Port
 *  @{
 */

/** @file
 * @brief Common stuff for porting layer.  These functions are thread-safe.
 *
 * Note: aside from calling uPortInit() at start of day, uPortDeinit() at
 * end of day, and uPortFree() if you are freeing some memory that ubxlib
 * has allocated, this API is NOT INTENDED FOR CUSTOMER USE.  You may use
 * it if you wish but it is quite restricted and is intended _only_ to
 * provide what ubxlib needs in the form that ubxlib needs it, internally
 * for ubxlib.  It is used in the ubxlib examples but that is only because
 * we need those examples to work on all of our supported platforms.  When
 * writing your application you are better off using the fully-featured
 * native APIs of your platform.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Used only by U_PORT_STRINGIFY_QUOTED.
 */
#define U_PORT_STRINGIFY_LITERAL(x) #x

/** Stringify a macro, so if you have:
 *
 * \#define foo bar
 *
 * ...U_PORT_STRINGIFY_QUOTED(foo) is "bar".
 */
#define U_PORT_STRINGIFY_QUOTED(x) U_PORT_STRINGIFY_LITERAL(x)

/** Endianness check: evaluates to 1 if this processor is little-endian,
 * else 0.
 */
#define U_PORT_IS_LITTLE_ENDIAN ((union {                 \
                                      uint16_t uint16;    \
                                      unsigned char c;    \
                                 }) {.uint16 = 1}.c)

/** Byte-reverse a uint64_t; may be required for endianness
 * conversion.  valueUint64 must be a uint64_t, lengthBytes of it
 * will be byte-reversed.
 */
#define U_PORT_BYTE_REVERSE(valueUint64, lengthBytes) {                 \
    uint64_t newValue;                                                  \
    size_t lengthReversed = lengthBytes;                                \
    if (lengthReversed > sizeof(newValue)) {                            \
        lengthReversed = sizeof(newValue);                              \
    }                                                                   \
    if (lengthReversed > 0) {                                           \
       uint8_t *pSrc = (uint8_t *) &valueUint64;                        \
       uint8_t *pDest = (((uint8_t *) &newValue) + lengthReversed - 1); \
       for (size_t x = 0; x < lengthReversed; x++) {                    \
           *pDest = *pSrc;                                              \
           pDest--;                                                     \
           pSrc++;                                                      \
       }                                                                \
       valueUint64 = newValue;                                          \
    }                                                                   \
}

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Start the platform.  This configures clocks, resources, etc.
 * and then calls pEntryPoint, i.e. the application, in an RTOS task.
 * This is used as a standard way to start the system for all of the
 * u-blox examples and all of the u-blox tests.
 *
 * You may have your own mechanism for initialisating the HW and
 * starting an RTOS task, in which case you need not use this
 * function.
 *
 * This function only returns if there is an error;
 * code execution ends up in pEntryPoint, which should
 * never return.
 *
 * @param[in] pEntryPoint the function to run.
 * @param[in] pParameter  a pointer that will be passed to pEntryPoint
 *                        when it is called. Usually NULL.
 * @param stackSizeBytes  the number of bytes of memory to dynamically
 *                        allocate for stack; ignored if the RTOS
 *                        has already been started and no new
 *                        task needs to be created for the entry point.
 * @param priority        the priority at which to run a task that
 *                        is pEntryPoint; ignored if the RTOS
 *                        has already been started and no new
 *                        task needs to be created for the entry point.
 * @return                negative error code.
 */
int32_t uPortPlatformStart(void (*pEntryPoint)(void *),
                           void *pParameter,
                           size_t stackSizeBytes,
                           int32_t priority);

/** Initialise the porting layer.  Should be called by
 * the application entry point before running any other
 * ubxlib function except uPortPlatformStart().
 * If the port is already initialised this function does
 * nothing and returns success, hence it can safely
 * be called at any time.
 *
 * @return zero on success else negative error code.
 */
int32_t uPortInit();

/** Deinitialise the porting layer.
 */
void uPortDeinit();

/** Get the current OS tick converted to a time in milliseconds.
 *
 * IMPORTANT: the value returned by this function should NOT
 * be used for checking time-outs or measuring delays; please
 * instead use uTimeoutStart(), the return value of which may be
 * passed to uTimeoutExpiredMs() or uTimeoutExpiredSeconds(),
 * time-out checking functions that know how to handle tick wraps.
 *
 * The return value of this function is guaranteed to be
 * unaffected by any time setting activity. It is NOT maintained
 * while the processor is in deep sleep, i.e. with clocks stopped;
 * port initialisation should be called on return from deep sleep
 * and that will restart this time from zero once more.
 *
 * @return the current OS tick converted to milliseconds.
 */
int32_t uPortGetTickTimeMs();

/** Get the heap high watermark, the minimum amount of heap
 * free, ever.
 *
 * @return the minimum amount of heap free in bytes or negative
 *         error code.
 */
int32_t uPortGetHeapMinFree();

/** Get the current free heap size.  This may be called at
 * any time, even before uPortInit() or after uPortDeinit().
 *
 * @return the amount of free heap in bytes or negative
 *         error code.
 */
int32_t uPortGetHeapFree();

/** Enter a critical section: no interrupts should go off, no
 * tasks will be rescheduled, until uPortExitCritical() is
 * called. Note that OS-related port APIs (i.e. uPortTask*,
 * uPortMutex*, uPortSemaphore*, uPortQueue*, uPortEventQueue*
 * or uPortTimer* functions) should NOT be called within the
 * critical section; depending on the platform that may cause
 * an assert or may cause the rescheduling you don't want to
 * happen anyway.  So don't do that.  Also, time may not pass,
 * i.e. uPortGetTickTimeMs() may not advance, during the critical
 * section.
 *
 * It is NOT a requirement that this API is implemented:
 * where it is not implemented #U_ERROR_COMMON_NOT_IMPLEMENTED
 * should be returned.  However, note that some features
 * (e.g. cellular power saving, which uses
 * uAtClientSetWakeUpHandler(), which uses this critical section
 * function) will not work if uPortEnterCritical() is not
 * implemented.
 *
 * @return zero on success else negative error code.
 */
int32_t uPortEnterCritical();

/** Leave a critical section: use this AS SOON AS POSSIBLE
 * after uPortEnterCritical().
 *
 * It is NOT a requirement that this API is implemented:
 * where it is not implemented #U_ERROR_COMMON_NOT_IMPLEMENTED
 * should be returned by uPortEnterCritical().
 */
void uPortExitCritical();

/** Get the current timezone offset (including daylight saving
 * time, where relevant).
 *
 * Note: the primary use of this function is to compensate for the
 * fact that mktime() assumes its input is in local time, not UTC,
 * and ends up subtracting a timezone offset from the result. If you
 * are calling mktime() with a UTC time then you can add the return
 * value of this function to that returned by mktime() to get back to
 * UTC.
 *
 * It is ONLY a requirement that this API is implemented if the
 * underlying system allows a non-zero timezone to be set: where it is
 * not implemented zero will be returned by a weakly-linked default
 * function.
 *
 * @return the current timezone offset in seconds.
 */
int32_t uPortGetTimezoneOffsetSeconds();

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_PORT_H_

// End of file
