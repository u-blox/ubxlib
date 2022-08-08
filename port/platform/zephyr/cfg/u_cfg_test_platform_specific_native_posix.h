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

#ifndef _U_CFG_TEST_PLATFORM_SPECIFIC_NATIVE_POSIX_H_
#define _U_CFG_TEST_PLATFORM_SPECIFIC_NATIVE_POSIX_H_

/** Pin A for GPIO testing: not supported on Linux so set to -1.
 */
#ifndef U_CFG_TEST_PIN_A
# define U_CFG_TEST_PIN_A         -1
#endif

/** Pin B for GPIO testing: not supported on Linux so set to -1.
 */
#ifndef U_CFG_TEST_PIN_B
# define U_CFG_TEST_PIN_B         -1
#endif

/** Pin C for GPIO testing:  not supported on Linux so set to -1.
 */
#ifndef U_CFG_TEST_PIN_C
# define U_CFG_TEST_PIN_C         -1
#endif

/** Reset pin for a GNSS module, not relevant on Linux
 * since it is only used for testing of I2C, which Linux doesn't
 * have.
 */
#ifndef U_CFG_TEST_PIN_GNSS_RESET_N
# define U_CFG_TEST_PIN_GNSS_RESET_N   -1
#endif

#endif // _U_CFG_TEST_PLATFORM_SPECIFIC_NATIVE_POSIX_H_

// End of file
