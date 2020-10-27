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

#ifndef _U_CELL_TEST_CFG_H_
#define _U_CELL_TEST_CFG_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines configuration values for cellular
 & API testing.
 */

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS
/** The time in seconds allowed for a connection to complete.
 */
# define U_CELL_TEST_CFG_CONNECT_TIMEOUT_SECONDS 240
#endif

#ifndef U_CELL_TEST_CFG_CONTEXT_ACTIVATION_TIMEOUT_SECONDS
/** The time in seconds allowed for context activation complete.
 */
# define U_CELL_TEST_CFG_CONTEXT_ACTIVATION_TIMEOUT_SECONDS 60
#endif

#ifndef U_CELL_TEST_CFG_SIM_PIN
/** The PIN to use during cellular testing.
 */
# define U_CELL_TEST_CFG_SIM_PIN NULL
#endif

#ifndef U_CELL_TEST_CFG_APN
/** The APN to use during cellular testing.
 */
# define U_CELL_TEST_CFG_APN NULL
#endif

#ifndef U_CELL_TEST_CFG_EUTRAN_APN
/** The test box that we use at u-blox for testing
 * cat-M1/NB1 does not allow registration without
 * an APN being supplied, and this is it.
 */
# define U_CELL_TEST_CFG_EUTRAN_APN "internet"
#endif

#ifndef U_CELL_TEST_CFG_USERNAME
/** The user name to use during cellular testing.
 */
# define U_CELL_TEST_CFG_USERNAME NULL
#endif

#ifndef U_CELL_TEST_CFG_PASSWORD
/** The password to use during cellular testing.
 */
# define U_CELL_TEST_CFG_PASSWORD NULL
#endif

#ifndef U_CELL_TEST_CFG_MNO_PROFILE
/** The MNO profile to use during testing: Europe (100),
 * which is good for the default band mask below.
 */
#define U_CELL_TEST_CFG_MNO_PROFILE 100
#endif

#ifndef U_CELL_TEST_CFG_BANDMASK1
/** The bandmask 1 to use during testing. 0x080092 is bands
 * 2, 5, 8 and 20.
 */
# define U_CELL_TEST_CFG_BANDMASK1   0x080092ULL
#endif

#ifndef U_CELL_TEST_CFG_BANDMASK2
/** The bandmask 2 to use during testing.
 */
# define U_CELL_TEST_CFG_BANDMASK2   0ULL
#endif

#endif // _U_CELL_TEST_CFG_H_

// End of file
