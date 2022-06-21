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

#ifndef _U_SECTIONS_GCC_H_
#define _U_SECTIONS_GCC_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files.. */

/** @file
 * @brief This file is internally used to map GCC linker sections.
 */

#ifndef CODE_END
extern const char _etext[]; // The most common name
# define CODE_END ((uint32_t)_etext)
#endif
#ifndef CODE_START
extern const char _start[];
# define CODE_START ((uint32_t)_start)
#endif

#endif // _U_SECTIONS_GCC_H_

// End of file
