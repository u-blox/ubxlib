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

#ifndef _U_CFG_HW_PLATFORM_SPECIFIC_H_
#define _U_CFG_HW_PLATFORM_SPECIFIC_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file contains hardware configuration information for
 * an NRF52 platform that is built into this porting code.  You may
 * override these values as necessary.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR NRF52: TIMERS/COUNTERS
 * -------------------------------------------------------------- */

/** The TIMER instance to use as a ticker.  Chosing 1 so as not
 * to conflict with SOFT_DEVICE where it is used.
 */
#ifndef U_CFG_HW_TICK_TIMER_INSTANCE
# define U_CFG_HW_TICK_TIMER_INSTANCE           1
#endif

/** It is a design constraint of the NRF52 UART that,
 * in order to implement continuous reception, a timer instance
 * is also required to count the UART received characters (since
 * the DMA only tells you when it has finished).  Up to two
 * instances of the UART driver may be created, this is
 * the timer instance that will be used if UARTE0 is selected.
 */
#ifndef U_CFG_HW_UART_COUNTER_INSTANCE_0
# define U_CFG_HW_UART_COUNTER_INSTANCE_0       2
#endif

/** It is a design constraint of the NRF52 UART that,
 * in order to implement continuous reception, a timer instance
 * is also required to count the UART received characters (since
 * the DMA only tells you when it has finished).  Up to two
 * instances of the UART driver may be created, this is
 * the timer instance that will be used if UARTE1 is selected.
 */
#ifndef U_CFG_HW_UART_COUNTER_INSTANCE_1
# define U_CFG_HW_UART_COUNTER_INSTANCE_1       3
#endif

#endif // _U_CFG_HW_PLATFORM_SPECIFIC_H_

// End of file
