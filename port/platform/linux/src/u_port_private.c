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

/** @file
 * @brief Stuff private to the Linux porting layer.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_cfg_os_platform_specific.h"
#include "u_error_common.h"
#include "u_assert.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_debug.h"
#include "u_port_private.h"


/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS SPECIFIC TO THIS PORT, LIST OF POINTERS
 * -------------------------------------------------------------- */

bool uPortPrivateListAdd(uPortPrivateList_t **ppList, void *ptr)
{
    if (ppList == NULL) {
        return false;
    }
    uPortPrivateList_t *pMember = pUPortMalloc(sizeof(uPortPrivateList_t));
    if (pMember == NULL) {
        return false;
    }
    pMember->ptr = ptr;
    pMember->pNext = NULL;
    if (*ppList == NULL) {
        *ppList = pMember;
    } else {
        uPortPrivateList_t *p = *ppList;
        while (p->pNext != NULL) {
            p = p->pNext;
        }
        p->pNext = pMember;
    }
    return true;
}

uPortPrivateList_t *uPortPrivateListFind(uPortPrivateList_t **ppList, void *ptr)
{
    uPortPrivateList_t *p = *ppList;
    while ((p != NULL) && (p->ptr != ptr)) {
        p = p->pNext;
    }
    return p;
}

bool uPortPrivateListRemove(uPortPrivateList_t **ppList, void *ptr)
{
    uPortPrivateList_t *pCurr = *ppList;
    uPortPrivateList_t *pPrev = pCurr;
    while ((pCurr != NULL)) {
        if (pCurr->ptr == ptr) {
            if (pCurr == *ppList) {
                *ppList = pCurr->pNext;
            } else {
                pPrev->pNext = pCurr->pNext;
            }
            uPortFree(pCurr);
            return true;
        }
        pPrev = pCurr;
        pCurr = pCurr->pNext;
    }
    return false;
}


// End of file
