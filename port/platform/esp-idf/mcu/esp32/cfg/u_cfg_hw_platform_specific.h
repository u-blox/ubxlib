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
 * an ESP32 platform that is built into this porting code.  You may
 * override these values as necessary.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS FOR A CELLULAR MODULE ON ESP32: MISC
 * -------------------------------------------------------------- */

#ifndef U_CFG_HW_CELLULAR_RTS_THRESHOLD
/** The buffer threshold at which RTS is de-asserted, indicating the
 * cellular module should stop sending data to us.  Must be defined
 * if U_CFG_APP_PIN_CELL_RTS is not -1.
 * Must be less than UART_FIFO_LEN, which is by default 128.
 */
# define U_CFG_HW_CELLULAR_RTS_THRESHOLD         100
#endif

#endif // _U_CFG_HW_PLATFORM_SPECIFIC_H_

// End of file
