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

#ifndef _U_PORT_PPP_PRIVATE_H_
#define _U_PORT_PPP_PRIVATE_H_

/** @file
 * @brief Stuff private to the PPP part of the Linux porting layer.
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

/** Initialise the PPP stuff.
 *
 * @return zero on success else negative error code.
 */
int32_t uPortPppPrivateInit();

/** Deinitialise the PPP stuff.
 */
void uPortPppPrivateDeinit();

#ifdef __cplusplus
}
#endif

#endif // _U_PORT_PPP_PRIVATE_H_

// End of file
