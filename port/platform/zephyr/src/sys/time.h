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
 * @brief The minimal library provided with Zephyr doesn't include a
 * definition for timeval so define it here.
 */

#ifndef _TIMEVAL_DEFINED
#define _TIMEVAL_DEFINED

#ifndef CONFIG_ARCH_POSIX
# include <sys/_types.h>
#else
# include <sys/types.h>
#endif

#if !defined(__time_t_defined)
#define __time_t_defined
typedef _TIME_T_ time_t;
#endif

#if !defined(__suseconds_t_defined)
#define __suseconds_t_defined
typedef _SUSECONDS_T_ suseconds_t;
#endif

/*
 * Structure returned by gettimeofday(2) system call, and used in other calls.
 */
#ifndef CONFIG_ARCH_POSIX
struct timeval {
    time_t       tv_sec;    /* seconds */
    suseconds_t  tv_usec;   /* and microseconds */
};
#else
# include <bits/types/struct_timeval.h>
#endif

#endif /* _TIMEVAL_DEFINED */

#endif /* !_SYS__TIMEVAL_H_ */
