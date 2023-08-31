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
 * @brief Linked list utilities.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_port_os.h"
#include "u_port_heap.h"

#include "u_linked_list.h"

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
 * FUNCTIONS
 * -------------------------------------------------------------- */

bool uLinkedListAdd(uLinkedList_t **ppList, void *p)
{
    if (ppList == NULL) {
        return false;
    }
    uLinkedList_t *pMember = pUPortMalloc(sizeof(uLinkedList_t));
    if (pMember == NULL) {
        return false;
    }
    pMember->p = p;
    pMember->pNext = NULL;
    if (*ppList == NULL) {
        *ppList = pMember;
    } else {
        uLinkedList_t *p = *ppList;
        while (p->pNext != NULL) {
            p = p->pNext;
        }
        p->pNext = pMember;
    }
    return true;
}

uLinkedList_t *pULinkedListFind(uLinkedList_t **ppList, void *p)
{
    uLinkedList_t *pList = NULL;
    if (ppList != NULL) {
        pList = *ppList;
    }
    while ((pList != NULL) && (pList->p != p)) {
        pList = pList->pNext;
    }
    return pList;
}

bool uLinkedListRemove(uLinkedList_t **ppList, void *p)
{
    uLinkedList_t *pCurr = NULL;
    if (ppList != NULL) {
        pCurr = *ppList;
    }
    uLinkedList_t *pPrev = pCurr;
    while ((pCurr != NULL)) {
        if (pCurr->p == p) {
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
