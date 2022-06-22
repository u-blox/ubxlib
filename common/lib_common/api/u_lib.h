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

#ifndef _U_LIB_H_
#define _U_LIB_H_

/** \addtogroup __lib-SHO __Library Internal
 *  @{
 */

/** @file
 * @brief ubxlib library handler API.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "stdarg.h"
#include "u_error_common.h"

/** Utility macro for getting the library context from library handle */
#define U_LIB_CTX(libHdl) ((libHdl)->ictx)

/** Indicates that the library is encrypted */
#define U_LIB_HDR_FLAG_ENCRYPTED            (1<<0)
/** Indicates that the library can be validated */
#define U_LIB_HDR_FLAG_VALIDATION           (1<<1)
/** Indicates that the library uses malloc and free */
#define U_LIB_HDR_FLAG_NEEDS_MALLOC         (1<<2)

/** On what bit position in flags the arch resides */
#define U_LIB_HDR_FLAG_ARCH_BITPOS          (4)
/** Flag mask for arch */
#define U_LIB_HDR_FLAG_ARCH_MASK            (0xff)
/** Flag mask bit for all arm architectures */
#define U_LIB_HDR_FLAG_ARCH_ARM_ID          (0x10)
/** Library flag for compiler architecture */
#if defined(__arm__)
#  if __ARM_ARCH == 6
#    define U_LIB_ARCH     ((U_LIB_HDR_FLAG_ARCH_ARM_ID+6) & U_LIB_HDR_FLAG_ARCH_MASK)
#  elif __ARM_ARCH == 7
#    define U_LIB_ARCH     ((U_LIB_HDR_FLAG_ARCH_ARM_ID+7) & U_LIB_HDR_FLAG_ARCH_MASK)
#  elif __ARM_ARCH == 8
#    define U_LIB_ARCH     ((U_LIB_HDR_FLAG_ARCH_ARM_ID+8) & U_LIB_HDR_FLAG_ARCH_MASK)
#  else
#    error "The ARM architecture you're compiling this library for is not yet considered. Please add a new identifier for it."
#  endif
#elif defined(__x86_64__)
#  define U_LIB_ARCH       (0x01 & U_LIB_HDR_FLAG_ARCH_MASK)
#elif defined(__i386__)
#  define U_LIB_ARCH       (0x02 & U_LIB_HDR_FLAG_ARCH_MASK)
#else
#  error "The architecture you're compiling this library for is not yet considered. Please add a new identifier for it."
#endif

#if U_LIB_ARCH == 0 // just for sanity
#  error "The architecture definition is bad (overflow?)"
#endif

/** Return 8-bit archituecture identifier from library flags */
#define U_LIB_HDR_FLAG_GET_ARCH(flags) (((flags) >> U_LIB_HDR_FLAG_ARCH_BITPOS) & U_LIB_HDR_FLAG_ARCH_MASK)

/**
 * Utility function pointers for the library. May be
 * NULL if the library does not need to access them.
 *
 * If more function pointers are needed for a specific library,
 * following construction can be used:
 *
 * @code
 * <code>
 * // file lib_foo.h
 * #include "u_lib_internal.h"
 * ...
 * typedef struct
 * {
 *   uLibLibc_t uliblibc;
 *   int (*fnstrcmp)(const char *s1, const char *s2);
 *   int (*fnmemcpy)(void *dst, const void *src, uint32_t num);
 * } uLibLibcExpanded_t;
 * ...
 * </code>
 * @endcode
 *
 * This must be handled in the library's open function, like so:
 *
 * @code
 * <code>
 * // file lib_foo.c
 * #include "lib_foo.h"
 * ...
 * int U_LIB_I_OPEN_FUNC(uLibLibc_t *pLibc, uint32_t flags, void **pCtx)
 * {
 *   uLibLibcExpanded_t *pLibcx = (uLibLibcExpanded_t *)pLibc;
 *   ...
 * }
 * ...
 * </code>
 * @endcode
 *
 * This way, the expanded struct can also be used by libraries using
 * the classic uLibLibc_t struct.
 */
typedef struct {
    /** malloc prototype, if the library needs to allocate memory */
    void *(*fnmalloc)(uint32_t size);
    /** free prototype, if the library needs to free allocated memory */
    void (*fnfree)(void *p);
    /** vprintf prototype, can be used for debug */
    int (*fnvprintf)(const char *format, va_list arg);
} uLibLibc_t;

/**
 * Library instance handle.
 */
typedef struct {
    /** Pointer to library descriptor */
    const void *puLibDescr;
    /** Pointer to library code */
    const void *puLibCode;
    /** Internal library context */
    void *ictx;
    /** Last error */
    int error;
} uLibHdl_t;

/**
 * Generic library header information
 */
typedef struct {
    /** Library name */
    const char *name;
    /** Library version */
    uint32_t version;
    /** Combinations of U_LIB_HDR_FLAG_x */
    uint32_t flags;
} uLibHdr_t;

/**
 * Reads library header.
 * @param pHdl Pointer to library handle struct, used to reference this library instance
 * This will be filled in by the function. Can be NULL.
 * @param pHdr Address of header struct to populate.
 * @param puLib Address of library blob
 * @return U_ERROR_COMMON_SUCCESS if OK, else error code
 */
int uLibProbe(uLibHdl_t *pHdl, uLibHdr_t *pHdr, const void *puLib);

/**
 * Opens given library.
 * @param pHdl Pointer to library handle struct, used to reference this library instance
 * This will be filled in by the function. Can be NULL.
 * @param puLib Address of library blob
 * @param pLibc Struct with pointers to utility functions. See uLibLibc_t for adding more functions.
 * @param flags Passed to library internal open function, ignored by handler
 * @param pRelocate Relocate the library code to this address. Use NULL if no relocation is needed.
 * @return U_ERROR_COMMON_SUCCESS if OK, else error code
 */
int uLibOpen(uLibHdl_t *pHdl, const void *puLib,
             uLibLibc_t *pLibc, uint32_t flags,
             void *pRelocate);

/**
 * Returns current location and size of the library executable code.
 * @param pHdl Pointer to library handle struct
 * @param pPtr Unless NULL, will be populated with address
 * @param pLen Unless NULL, will be length
 * @return U_ERROR_COMMON_SUCCESS if OK, else error code
 */
int uLibGetCode(uLibHdl_t *pHdl, const void **pPtr, uint32_t *pLen);

/**
 * Points out new address for the code.
 * Useful when e.g. decrypting library code before use.
 * @param pHdl Pointer to library handle struct
 * @param dst Where the code resides.
 * @return U_ERROR_COMMON_SUCCESS if OK, else error code
 */
int uLibRelocate(uLibHdl_t *pHdl, void *dst);

/**
 * Closes given library.
 * @param pHdl Pointer to library handle struct
 * @return U_ERROR_COMMON_SUCCESS if OK, else error code
 */
int uLibClose(uLibHdl_t *pHdl);

/**
 * Returns call address for given symbol or NULL on error. If NULL is returned,
 * see function uLibError.
 * @param pHdl Pointer to library handle struct.
 * @param sym Function symbol name to find.
 * @return Address to function, or NULL on error
 */
void *uLibSym(uLibHdl_t *pHdl, const char *sym);

/**
 * Returns and clears last error for given library.
 * @param pHdl Pointer to library handle struct.
 * @return Last error.
 */
int uLibError(uLibHdl_t *pHdl);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_LIB_H_
