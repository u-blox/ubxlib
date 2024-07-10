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

#ifndef _U_PORT_PRIVATE_H_
#define _U_PORT_PRIVATE_H_

/** @file
 * @brief Stuff private to the STM32 porting layer.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Get the port number of a pin, which is the upper nibble.
 */
#define U_PORT_STM32F4_GPIO_PORT(x) ((uint16_t ) (((uint32_t) x) >> 4))

/** The generic version of #U_PORT_STM32F4_GPIO_PORT, for use on
 * non-F4 MCUs.
*/
#define U_PORT_STM32_GPIO_PORT U_PORT_STM32F4_GPIO_PORT

/** Get the pin number of a pin, which is the lower nibble.
 */
#define U_PORT_STM32F4_GPIO_PIN(x) ((uint16_t ) (x & 0x0f))

/** The generic version of #U_PORT_STM32F4_GPIO_PIN, for use on
 * non-F4 MCUs.
*/
#define U_PORT_STM32_GPIO_PIN U_PORT_STM32F4_GPIO_PIN

#ifndef U_PORT_PRIVATE_TIMER_NAME_MAX_LEN_BYTES
/** The maximum length of the name of a timer: the name is used for
 * diagnostic purposes only so it is not allowed to be very long
 * to save on RAM.
  */
# define U_PORT_PRIVATE_TIMER_NAME_MAX_LEN_BYTES 8
#endif

#if defined(U_PORT_STM32_PURE_CMSIS) && !defined(U_PORT_STM32_CMSIS_ON_FREERTOS)
/** Convert a millisecond value to an RTOS tick, ThreadX case.
 */
# define MS_TO_TICKS(delayMs)  ((TX_TIMER_TICKS_PER_SECOND * delayMs + 500 ) / 1000)
#else
/** Convert a millisecond value to an RTOS tick, FreeRTOS case.
 */
# define MS_TO_TICKS(delayMs)  ((configTICK_RATE_HZ * delayMs + 500) / 1000)
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS: INIT
 * -------------------------------------------------------------- */

/** Initialise the private stuff.
 *
 * @return zero on success else negative error code.
 */
int32_t uPortPrivateInit();

/** Deinitialise the private stuff.
 */
void uPortPrivateDeinit();

/* ----------------------------------------------------------------
 * FUNCTIONS: TIMERS
 * -------------------------------------------------------------- */

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

/** Start a CMSIS-based timer.  This is a private function,
 * unlike the non-CMSIS case, since the timer duration has to
 * be pulled from the linked list entry we stored when it
 * was created (the CMSIS API does not remember the timer
 * duration at the point of creation).
 *
 * @param handle  the handle of the timer to be started.
 * @return        zero on success else negative error code.
 */
int32_t uPortPrivateTimerStartCmsis(const uPortTimerHandle_t handle);

/** Change the duration of a CMSIS-based timer.  This does
 * not modify the expiry time of a timer that is already
 * running, only the expiry time after the timer has next
 * been started; please stop the timer first to be sure.
 *
 * @param handle     the handle of the timer.
 * @param intervalMs the new time interval in milliseconds.
 * @return           zero on success else negative error code.
 */
int32_t uPortPrivateTimerChangeCmsis(const uPortTimerHandle_t handle,
                                     uint32_t intervalMs);

/** Remove a timer entry from the list.
 *
 * @param handle  the handle of the timer to be removed.
 * @return        zero on success else negative error code.
 */
int32_t uPortPrivateTimerDelete(const uPortTimerHandle_t handle);

/* ----------------------------------------------------------------
 * FUNCTIONS: SEMAPHORES FOR CMSIS
 * -------------------------------------------------------------- */

/** Create a semaphore, CMSIS case.
 *
 * Note: how can this be so confusing?  Just two unsigned integers
 * with seemingly meaningful names.  Yet it is.
 *
 * The reason for this set of CMSIS-specific functions is that one
 * of the RTOS's we support under CMSIS, ThreadX, has a semaphore
 * API that takes just one parameter, which it names "initial count".
 * A semaphore starts out with that number available and can be taken
 * that number of times before it is no longer take-able; so far so
 * good.  The problem is that ThreadX allows infinite "gives":
 * a semaphore of size 2 can be given 10 times and, as a result,
 * be taken 10 times afterwards, it is not possible to set an upper
 * limit.  This layer is required to add that upper limit.
 *
 * @param pSemaphoreHandle a place to put the semaphore handle.
 * @param initialCount     initial semaphore count.
 * @param limit            maximum permitted semaphore count.
 * @return                 zero on success else negative error code.
 */
int32_t uPortPrivateSemaphoreCreateCmsis(uPortSemaphoreHandle_t *pSemaphoreHandle,
                                         uint32_t initialCount,
                                         uint32_t limit);

/** Destroy a semaphore, CMSIS case.
 *
 * @param semaphoreHandle the handle of the semaphore.
 * @return                zero on success else negative error code.
 */
int32_t uPortPrivateSemaphoreDeleteCmsis(const uPortSemaphoreHandle_t semaphoreHandle);

/** Take the given semaphore, CMSIS case, waiting until it is
 * available if it is already taken.
 *
 * @param semaphoreHandle  the handle of the semaphore.
 * @return                 zero on success else negative error code.
 */
int32_t uPortPrivateSemaphoreTakeCmsis(const uPortSemaphoreHandle_t semaphoreHandle);

/** Try to take the given semaphore, CMSIS case, waiting up to
 * delayMs if it is currently taken.
 *
 * @param semaphoreHandle  the handle of the semaphore.
 * @param delayMs          the maximum time to wait in milliseconds.
 * @return                 zero on success else negative error code.
 */
int32_t uPortPrivateSemaphoreTryTakeCmsis(const uPortSemaphoreHandle_t semaphoreHandle,
                                          int32_t delayMs);

/** Give a semaphore, CMSIS case, unless the semaphore is already at
 * its maximum permitted  count.
 *
 * @param semaphoreHandle  the handle of the semaphore.
 * @return                 zero on success else negative error code.
 */
int32_t uPortPrivateSemaphoreGiveCmsis(const uPortSemaphoreHandle_t semaphoreHandle);

/* ----------------------------------------------------------------
 * FUNCTIONS: GET TIME TICK
 * -------------------------------------------------------------- */

/** Get the current OS tick converted to a time in milliseconds.
 */
int64_t uPortPrivateGetTickTimeMs();

/* ----------------------------------------------------------------
 * FUNCTIONS SPECIFIC TO THIS PORT: CRITICAL SECTION FOR CMSIS
 * -------------------------------------------------------------- */

/** Enter a critical section, CMSIS-wise.
 *
 * @return zero on success else negative error code.
 */
int32_t uPortPrivateEnterCriticalCmsis();

/** Leave a critical section, CMSIS-wise.
 */
void uPortPrivateExitCriticalCmsis();

/* ----------------------------------------------------------------
 * FUNCTIONS: MISC
 * -------------------------------------------------------------- */

/** Return the address of the port register for a given GPIO pin.
 *
 * @param pin the pin number.
 * @return    the GPIO port address.
 * */
GPIO_TypeDef *pUPortPrivateGpioGetReg(int32_t pin);

/** Enable the clock to the register of the given GPIO pin.
 *
 * @param pin the pin number.
 */
void uPortPrivateGpioEnableClock(int32_t pin);

#ifdef __cplusplus
}
#endif

#endif // _U_PORT_PRIVATE_H_

// End of file
