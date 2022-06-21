/*
 * Common and shared functions used by multiple modules in the Mbed TLS
 * library.
 *
 *  Copyright (C) 2018, Arm Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of Mbed TLS (https://tls.mbed.org)
 */

#include "stddef.h"

/* Note: NRF5 SDK version 17 uses a different version of mbedtls to
 * NRF5 SDK version 16, requiring an additional file, platform_utils.c
 * to be built from the mbedtls directory, which of course is not
 * present in version 16.  To maintain compatibility with both SDK
 * versions we instead take just the one function it needs,
 * mbedtls_platform_zeroize() and include it here.
 */

/* Implementation that should never be optimized out by the compiler */
void mbedtls_platform_zeroize(void *v, size_t n)
{
    volatile unsigned char *p = v;

    while (n--) {
        *p++ = 0;
    }
}