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

#ifndef _U_SECTIONS_ZEPHYR_H_
#define _U_SECTIONS_ZEPHYR_H_

#include <version.h>

/* No other #includes allowed here. */
#if KERNEL_VERSION_NUMBER >= ZEPHYR_VERSION(3,1,0)
#include <zephyr/linker/linker-defs.h>
#else
#include <linker/linker-defs.h>
#endif

/** @file
 * @brief This file is internally used to map Zephyr linker sections.
 */

#ifndef CODE_START
#define CODE_START ((uint32_t)__text_region_start)
#endif

#ifndef CODE_END
#define CODE_END ((uint32_t)__text_region_end)
#endif

#endif // _U_SECTIONS_ZEPHYR_H_

// End of file
