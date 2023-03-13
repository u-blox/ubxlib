/*
 * Copyright 2019-2023 u-blox
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

/** @file
 * @brief Workaround for Espressif linker missing out files that
 * only contain functions which also have weak alternatives
 * (see https://www.esp32.com/viewtopic.php?f=13&t=8418&p=35899).
 * The workaround is to introduce a function to a "wanted" .c file
 * that has no WEAK alternative.  However, when that .c file is left
 * out, for the stubs to take over, the code will then obviously
 * fail to link.  This file must be included when BLE is being
 * left out of the build.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h"  // For a customer's configuration override
#endif

/* ----------------------------------------------------------------
 * PUBLIC FUNCTION
 * -------------------------------------------------------------- */

void uNetworkPrivateBleLink()
{
    //dummy
}

// End of file
