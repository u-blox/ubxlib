/*
 * Copyright 2020 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _U_NETWORK_PRIVATE_SHORT_RANGE_H_
#define _U_NETWORK_PRIVATE_SHORT_RANGE_H_

/* No #includes allowed here */

/* This header file defines the short range specific part of the
 * network API. These functions perform NO error checking
 * and are NOT thread-safe; they should only be called from
 * within the network API which sorts all that out.
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

typedef struct {
    int32_t module; /**< The module type that is connected,
                         see uShortRangeModuleType_t in u_short_range.h. */
    int32_t uart; /**< The UART HW block to use. */
    int32_t pinTxd; /** The output pin that sends UART data to
                        the cellular module. */
    int32_t pinRxd; /** The input pin that receives UART data from
                        the cellular module. */
    int32_t pinCts; /**< The input pin that the cellular module
                         will use to indicate that data can be sent
                         to it; use -1 if there is no such connection. */
    int32_t pinRts; /**< The output pin output pin that tells the
                         cellular module that it can send more UART
                         data; use -1 if there is no such connection. */
} uShortRangeConfig_t;


/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Initialise the network API for short range.
 * May be called multiple times.
 *
 * @return  zero on success else negative error code.
 */
int32_t uNetworkInitShortRange(void);

/** Deinitialise the short range network API.
 * May be called multiple times. A reference counter is used to keep track
 * of number of calls to uNetworkInitShortRange(). When this function
 * is called reference counter will decrement and only when the counter
 * reaches 0 the real de-initialization will happen. BEFORE this happen
 * all short range network instances must have been removed
 * with a call to uNetworkRemoveShortRange().
 */
void uNetworkDeinitShortRange(void);

/** Add a short range network instance.
 * This function will open a UART port according to input config.
 * If the function is called multiple times with the same config
 * the UART port will only be open for the first call and all
 * succeeding calls will return the same handle.
 * Note: uNetworkInitShortRange() must have been called before using
 * this function.
 *
 * @param pConfiguration   a pointer to the configuration.
 * @return                 on success the handle of the
 *                         instance, else negative error code.
 */
int32_t uNetworkAddShortRange(const uShortRangeConfig_t *pConfiguration);

/** Remove a short range network instance.
 * Please note that when uNetworkAddShortRange() has been called
 * multiple times with the same config, uNetworkRemoveShortRange()
 * must be called the same amount of time until the UART is closed.
 * Notes: It is up to the caller to ensure that the network is
 * disconnected and/or powered down etc.; all this function does is
 * remove the logical instance. uNetworkInitShortRange() must have been
 * called before using this function.
 *
 * @param handle  the handle of the short range instance to remove.
 * @return        zero on success else negative error code.
 */
int32_t uNetworkRemoveShortRange(int32_t handle);

/** Get the AT client.
 *
 * @param handle  the handle of the short range instance.
 * @return        AT client handle on success else NULL.
 */
uAtClientHandle_t uNetworkGetAtClientShortRange(int32_t handle);

#ifdef __cplusplus
}
#endif

#endif // _U_NETWORK_PRIVATE_SHORT_RANGE_H_

// End of file
