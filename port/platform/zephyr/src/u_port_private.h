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
 * @brief Stuff private to the Zephyr porting layer.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise the os private stuff.
 */
void uPortOsPrivateInit();

/** Deinitialise the os private stuff.
 */
void uPortOsPrivateDeinit();

/** Initialise the private bits of the porting layer.
 *
 * @return: zero on success else negative error code.
 */
int32_t uPortPrivateInit(void);

/** Deinitialise the private bits of the porting layer.
 */
void uPortPrivateDeinit(void);

/** Add a timer entry to the list.  IMPORTANT: pCallback
 * is executed in the Zephyr system queue and hence it is
 * important that the user does not pass blocking calls to
 * the Zephyr system queue as that will effectively delay
 * timer expiry.
 *
 * @param pHandle         a place to put the timer handle.
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

/** Start a timer.
 *
 * @param handle       the handle of the timer.
 * @return             zero on success else negative error code.
 */
int32_t uPortPrivateTimerStart(const uPortTimerHandle_t handle);

/** Change a timer interval.
 *
 * @param handle       the handle of the timer.
 * @param intervalMs   the new time interval in milliseconds.
 * @return             zero on success else negative error code.
 */
int32_t uPortPrivateTimerChange(const uPortTimerHandle_t handle,
                                uint32_t intervalMs);

#ifdef __cplusplus
}
#endif

#endif // _U_PORT_PRIVATE_H_

// End of file
