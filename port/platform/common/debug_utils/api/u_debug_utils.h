/*
 * Copyright 2022 u-blox
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

#ifndef _U_DEBUG_UTILS_H_
#define _U_DEBUG_UTILS_H_

/* No #includes allowed here. */

/** @file
 * @brief Various debug utilities
 *
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
 * FUNCTIONS
 * -------------------------------------------------------------- */

#ifdef U_DEBUG_UTILS_DUMP_THREADS

/** Dump all current threads.
 *
 * This will print out name and state (if available) for each
 * thread together with a PC backtrace. The PC based backtrace
 * can be converted to a real backtrace by using addr2line.
 *
 * Example output:
 *   ### Dumping threads ###
 *     timerEvent (pending): bottom: 200064e0, top: 20006ce0, sp: 20006bd8
 *       Backtrace: 0x00050e16 0x0004e68a 0x0005c910 0x0005a1b6 0x0005a196 0x0005a196 0x0005d724
 *     sysworkq (pending): bottom: 200289a0, top: 200291a0, sp: 20029120
 *       Backtrace: 0x00050e16 0x000525d4 0x0004fe8c 0x0005d724
 */
void uDebugUtilsDumpThreads(void);


#endif

#ifdef __cplusplus
}
#endif

#endif // _U_DEBUG_UTILS_H_

// End of file
