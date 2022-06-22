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

#ifndef _U_LIB_INTERNAL_H_
#define _U_LIB_INTERNAL_H_

/** \addtogroup __lib-SHO
 *  @{
 */

/** @file
 * @brief ubxlib library, internal types.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "u_lib.h"

/** ubxlib header identifier */
#define U_LIB_I_MAGIC                      (0xc01df00d)

/* Function table entry flags */
/** Callable library function */
#define U_LIB_I_FDESC_FLAG_FUNCTION        (1<<0)
/** Library initialiser function */
#define U_LIB_I_FDESC_FLAG_INIT            (1<<1)
/** Library finaliser function */
#define U_LIB_I_FDESC_FLAG_FINI            (1<<2)

/** ubxlib initialiser function name, recognised by python script genlibhdr.py */
#define U_LIB_I_OPEN_FUNC                  ___libOpen
/** ubxlib finaliser function name, recognised by python script genlibhdr.py */
#define U_LIB_I_CLOSE_FUNC                 ___libClose

/**
 * Library open function prototype.
 * This functon is expected to be implemented in the library, with function
 * name ULIB_LIB_OPEN_FUNC.
 * @param pLibc Pointer to struct with utility function pointers. Some libraries
 *              may not need this, so the argument can be NULL. Other libraries
 *              may not need all functions, so respective pointers can be NULL.
 * @param flags Flags to library being opened.
 * @param pCtx Populated by library open function, used as an internal handle.
 * @return U_ERROR_COMMON_SUCCESS if opened successfully, else error
 */
typedef int (*ulibOpenFn_t)(uLibLibc_t *pLibc, uint32_t flags, void **pCtx);

/**
 * Library close function prototype.
 * This functon is expected to be implemented in the library, with function
 * name ULIB_LIB_CLOSE_FUNC.
 * @param ctx Internal handle populated by open function.
 */
typedef void (*ulibCloseFn_t)(void *ctx);

/**
 * Function descriptor, indicating symbol name and whereabouts
 * for function code. Can also be used as key/value pairs to
 * describe future (unforeseen) properties of the library.
 */
typedef struct {
    union {
        /* in case of function, this denotes the functions offset counted from end of library header */
        uint32_t offset;
        /* in case of not function, this is a generic value */
        uint32_t value;
    };
    /* function descriptor flags, combinations of ULIB_LIB_FDESC_FLAG_* */
    uint32_t flags;
    /* function descriptor name */
    const char name[40];
} _uLibFunctionDescriptor_t;

/**
 * Library header
 */
typedef struct {
    /* magic number, must be U_LIB_I_RARY_MAGIC to be a valid header */
    uint32_t magic;
    /* version number of library */
    uint32_t version;
    /* library flags, combinations of U_LIB_I_HDR_FLAG_x */
    uint32_t flags;
    /* number of function definitions following this header */
    uint32_t count;
    /* size of code blob in bytes */
    uint32_t length;
    /* library name */
    const char name[32];
} _uLibHeader_t;

/**
 * Library descriptor, comprised of a header followed
 * by an array of function descriptors.
 */
typedef struct {
    _uLibHeader_t hdr;
    _uLibFunctionDescriptor_t funcs[];
} uLibDescriptor_t;

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_LIB_INTERNAL_H_
