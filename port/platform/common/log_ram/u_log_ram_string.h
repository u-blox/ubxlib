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

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

// Extern the variable that gives the size of the log string array

#ifndef _U_LOG_RAM_STRING_H_
#define _U_LOG_RAM_STRING_H_

/* ----------------------------------------------------------------
 * EXTERNS
 * -------------------------------------------------------------- */

extern const char *gULogRamString[];
extern const size_t gULogRamNumStrings;

#endif // _U_LOG_RAM_STRING_H_
