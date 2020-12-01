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

#ifndef _U_SHORT_RANGE_AT_COMMANDS_H_
#define _U_SHORT_RANGE_AT_COMMANDS_H_

/* No #includes allowed here */

/** @file
 * @brief This header file for AT commands, only include this file from u_short_range.h.
 */

#ifdef __cplusplus
extern "C" {
#endif

int32_t getBleRole(const uAtClientHandle_t atHandle);
int32_t setBleRole(const uAtClientHandle_t atHandle, int32_t role);
int32_t getServers(const uAtClientHandle_t atHandle, uShortRangeServerType_t type);
int32_t setServer(const uAtClientHandle_t atHandle, uShortRangeServerType_t type);
int32_t restart(const uAtClientHandle_t atHandle, bool store);
int32_t setEchoOff(const uAtClientHandle_t atHandle, uint8_t retries);
uShortRangeModuleType_t getModule(const uAtClientHandle_t atHandle);

#ifdef __cplusplus
}
#endif

#endif // _U_SHORT_RANGE_AT_COMMANDS_H_

// End of file
