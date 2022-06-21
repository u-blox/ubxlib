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
 * @brief generic ubxlib library handler.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "u_lib.h"
#include "u_lib_internal.h"
#include "u_error_common.h"
#include "string.h"

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

static uint8_t *getCallAddress(uLibHdl_t *pHdl, uint32_t funcIx)
{
    uLibDescriptor_t *pDescr = (uLibDescriptor_t *)pHdl->puLibDescr;
    uint8_t *pFunc = (uint8_t *)pHdl->puLibCode + pDescr->funcs[funcIx].offset;
#ifdef __thumb__
    // On thumb2, always force odd jumps or we'll switch to arm
    // givin a hardfault
    pFunc = (uint8_t *)((intptr_t) pFunc | 1);
#endif
    return pFunc;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */
int uLibProbe(uLibHdl_t *pHdl, uLibHdr_t *pHdr, const void *puLib)
{
    uLibDescriptor_t *pDescr = (uLibDescriptor_t *) puLib;
    if (pHdr == 0 || pDescr == 0 || pDescr->hdr.magic != U_LIB_I_MAGIC) {
        return U_ERROR_COMMON_INVALID_PARAMETER;
    }
    pHdr->flags = pDescr->hdr.flags;
    pHdr->name = pDescr->hdr.name;
    pHdr->version = pDescr->hdr.version;

    if (pHdl != NULL) {
        pHdl->puLibDescr = puLib;
        pHdl->puLibCode = (void *)(&pDescr->funcs[pDescr->hdr.count]);
    }
    return U_ERROR_COMMON_SUCCESS;
}

int uLibOpen(uLibHdl_t *pHdl, const void *puLib,
             uLibLibc_t *pLibc, uint32_t flags,
             void *pRelocate)
{
    int res = U_ERROR_COMMON_SUCCESS;
    uLibDescriptor_t *pDescr = (uLibDescriptor_t *) puLib;
    if (pHdl == 0 || pDescr == 0 || pDescr->hdr.magic != U_LIB_I_MAGIC) {
        return U_ERROR_COMMON_INVALID_PARAMETER;
    }
    if (U_LIB_HDR_FLAG_GET_ARCH(pDescr->hdr.flags) != U_LIB_ARCH) {
        // this library was compiled for another architecture
        return U_ERROR_COMMON_INVALID_PARAMETER;
    }
    pHdl->puLibDescr = puLib;

    if (pRelocate != NULL) {
        pHdl->puLibCode = pRelocate;
    } else {
        pHdl->puLibCode = (void *)(&pDescr->funcs[pDescr->hdr.count]);
    }
    for (uint32_t i = 0; i < pDescr->hdr.count; i++) {
        if ((pDescr->funcs[i].flags & (U_LIB_I_FDESC_FLAG_INIT | U_LIB_I_FDESC_FLAG_FUNCTION))
            == (U_LIB_I_FDESC_FLAG_INIT | U_LIB_I_FDESC_FLAG_FUNCTION)) {
            res = ((ulibOpenFn_t)getCallAddress(pHdl, i))(pLibc, flags, &pHdl->ictx);
            if (res != U_ERROR_COMMON_SUCCESS) {
                break;
            }
        }
    }
    pHdl->error = res;
    return res;
}

int uLibGetCode(uLibHdl_t *pHdl, const void **pPtr, uint32_t *pLen)
{
    if (pHdl == 0) {
        return U_ERROR_COMMON_INVALID_PARAMETER;
    }
    if (pHdl->puLibDescr == 0) {
        return U_ERROR_COMMON_NOT_INITIALISED;
    }
    uLibDescriptor_t *pDescr = (uLibDescriptor_t *)pHdl->puLibDescr;

    if (pPtr) {
        *pPtr = pHdl->puLibCode;
    }
    if (pLen) {
        *pLen = pDescr->hdr.length;
    }
    return U_ERROR_COMMON_SUCCESS;
}

int uLibRelocate(uLibHdl_t *pHdl, void *dst)
{
    if (pHdl == 0 || dst == 0) {
        return U_ERROR_COMMON_INVALID_PARAMETER;
    }
    if (pHdl->puLibDescr == 0) {
        return U_ERROR_COMMON_NOT_INITIALISED;
    }
    pHdl->puLibCode = dst;
    return U_ERROR_COMMON_SUCCESS;
}

int uLibClose(uLibHdl_t *pHdl)
{
    if (pHdl == 0) {
        return U_ERROR_COMMON_INVALID_PARAMETER;
    }
    if (pHdl->puLibDescr == 0) {
        return U_ERROR_COMMON_SUCCESS; // already closed
    }

    uLibDescriptor_t *pDescr = (uLibDescriptor_t *)pHdl->puLibDescr;
    for (uint32_t i = 0; i < pDescr->hdr.count; i++) {
        if ((pDescr->funcs[i].flags & (U_LIB_I_FDESC_FLAG_FINI | U_LIB_I_FDESC_FLAG_FUNCTION))
            == (U_LIB_I_FDESC_FLAG_FINI | U_LIB_I_FDESC_FLAG_FUNCTION)) {
            ((ulibCloseFn_t)getCallAddress(pHdl, i))(pHdl->ictx);
        }
    }

    pHdl->puLibDescr = 0; // indicate closed by nulling library descriptor pointer

    return U_ERROR_COMMON_SUCCESS;
}

void *uLibSym(uLibHdl_t *pHdl, const char *sym)
{
    if (pHdl == 0) {
        return 0;
    }
    if (pHdl->puLibDescr == 0) {
        pHdl->error = U_ERROR_COMMON_NOT_INITIALISED;
        return 0;
    }
    if (sym == 0) {
        pHdl->error = U_ERROR_COMMON_INVALID_PARAMETER;
        return 0;
    }

    uLibDescriptor_t *pDescr = (uLibDescriptor_t *)pHdl->puLibDescr;
    for (uint32_t i = 0; i < pDescr->hdr.count; i++) {
        if (((pDescr->funcs[i].flags & (U_LIB_I_FDESC_FLAG_INIT | U_LIB_I_FDESC_FLAG_FINI |
                                        U_LIB_I_FDESC_FLAG_FUNCTION))
             == U_LIB_I_FDESC_FLAG_FUNCTION) &&
            strcmp(sym, pDescr->funcs[i].name) == 0) {
            return (void *)getCallAddress(pHdl, i);
        }
    }
    pHdl->error = U_ERROR_COMMON_NOT_FOUND;
    return 0;
}

int uLibError(uLibHdl_t *pHdl)
{
    if (pHdl == 0) {
        return U_ERROR_COMMON_INVALID_PARAMETER;
    }
    int err = pHdl->error;
    pHdl->error = U_ERROR_COMMON_SUCCESS;
    return err;
}
