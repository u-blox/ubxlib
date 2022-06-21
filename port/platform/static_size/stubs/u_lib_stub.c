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

/** @file
 * @brief Stubs for library functions normally filled-in by the platform.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.

int _kill()
{
    return 0;
}
int _getpid()
{
    return 0;
}
char *_sbrk(int incr)
{
    (void) incr;
    return NULL;
}
int _read(int fildes, void *buf, size_t nbyte)
{
    (void) fildes;
    (void) buf;
    (void) nbyte;
    return 0;
}
int _write(int fildes, const void *buf, size_t nbyte)
{
    (void) fildes;
    (void) buf;
    (void) nbyte;
    return 0;
}
int _lseek(int fildes, int offset, int whence)
{
    (void) fildes;
    (void) offset;
    (void) whence;
    return 0;
}
int _isatty(int fildes)
{
    (void) fildes;
    return 0;
}
int _fstat(int fildes, void *st)
{
    (void) fildes;
    (void) st;
    return 0;
}
int _close(int fildes)
{
    (void) fildes;
    return 0;
}
// End of file
