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

#ifndef _U_SPARTN_TEST_DATA_H_
#define _U_SPARTN_TEST_DATA_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup __spartn __SPARTN
 *  @{
 */

/** @file
 * @brief Extern the SPARTN test data.
 */

/* ----------------------------------------------------------------
 * EXTERNS
 * -------------------------------------------------------------- */

/** The SPARTN test frames captured from MQTT.
 */
extern const char gUSpartnTestData[];

/** The size of #gUSpartnTestData.
 */
extern size_t const gUSpartnTestDataSize;

/** The number of SPARTN messges in #gUSpartnTestData.
 */
extern size_t const gUSpartnTestDataNumMessages;

/** @}*/

#endif // _U_SPARTN_TEST_DATA_H_

// End of file
