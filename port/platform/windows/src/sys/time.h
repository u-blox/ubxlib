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

#ifndef _SYS__TIMEVAL_H_
#define _SYS__TIMEVAL_H_

/** @file
 * @brief MSVC doesn't include a definition for timeval so
 * define it here.
 */

/*
 * Structure returned by gettimeofday(2) system call, and used in other calls.
 */
struct timeval {
    time_t       tv_sec;    /* seconds */
    long         tv_usec;   /* and microseconds */
} timeval;

#endif /* !_SYS__TIMEVAL_H_ */
