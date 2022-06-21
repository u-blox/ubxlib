/*
 * Copyright 2019-2022 u-blox
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

#ifndef _U_PORT_PRIVATE_H_
#define _U_PORT_PRIVATE_H_

/** @file
 * @brief Stuff private to the ESP32 porting layer.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_PRIVATE_TIMER_NAME_MAX_LEN_BYTES
/** The maximum length of the name of a timer: the name is used for
 * diagnostic purposes only so it is not allowed to be very long
 * to save on RAM.
  */
# define U_PORT_PRIVATE_TIMER_NAME_MAX_LEN_BYTES 8
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise the private stuff.
 *
 * @return zero on success else negative error code.
 */
int32_t uPortPrivateInit();

/** Deinitialise the private stuff.
 */
void uPortPrivateDeinit();

/** Add a timer entry to the list.
 *
 * @param pHandle         a place to put the timer handle.
 * @param pName           a name for the timer, used for debug
 *                        purposes only; should be a null-terminated
 *                        string, may be NULL.  The value will be
 *                        copied.
 * @param pCallback       the timer callback routine.
 * @param pCallbackParam  a parameter that will be provided to the
 *                        timer callback routine as its second parameter
 *                        when it is called; may be NULL.
 * @param intervalMs      the time interval in milliseconds.
 * @param periodic        if true the timer will be restarted after it
 *                        has expired, else the timer will be one-shot.
 * @return                zero on success else negative error code.
 */
int32_t uPortPrivateTimerCreate(uPortTimerHandle_t *pHandle,
                                const char *pName,
                                pTimerCallback_t *pCallback,
                                void *pCallbackParam,
                                uint32_t intervalMs,
                                bool periodic);

/** Remove a timer entry from the list.
 *
 * @param handle  the handle of the timer to be removed.
 * @return        zero on success else negative error code.
 */
int32_t uPortPrivateTimerDelete(const uPortTimerHandle_t handle);

#ifdef __cplusplus
}
#endif

#endif // _U_PORT_PRIVATE_H_

// End of file
