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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of generic "interface" type helper functions.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // memset()/memcpy()

#include "u_error_common.h"
#include "u_port_os.h"
#include "u_port_heap.h"

#include "u_interface.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** The amount of memory needed for an interface.  The memory is
 * organised as follows:
 *
 *   +------------------------------------+
 *   |           uInterface_t             |
 *   +------------------------------------+
 *   |           VECTOR TABLE             |
 *   +------------------------------------+
 *   |             context                |
 *   +------------------------------------+
 *
 */
#define U_INTERFACE_SIZE_BYTES(sizeVectorTableBytes, sizeContextBytes) (sizeof(uInterface_t) +  \
                                                                       (sizeVectorTableBytes) + \
                                                                       (sizeContextBytes))

/** Get the address of the vector table from pInterface.
 */
#define U_INTERFACE_P_VECTOR_TABLE_P_INTERFACE(pInterface) ((void *) (((char *) (pInterface)) + \
                                                                      sizeof(uInterface_t)))

/** Get pInterface from the vector table address.
 */
#define U_INTERFACE_P_INTERFACE_P_VECTOR_TABLE(pVectorTable) ((uInterface_t *) (((char *) (pVectorTable)) - \
                                                                                sizeof(uInterface_t)))

/** Get the address of the context given the address of the vector table.
 */
#define U_INTERFACE_P_CONTEXT_P_VECTOR_TABLE(pVectorTable) ((void *) (((char *) (pVectorTable)) +                                                \
                                                                      U_INTERFACE_P_INTERFACE_P_VECTOR_TABLE(pVectorTable)->sizeVectorTableBytes))

/** Get the address of the context given pInterface.
 */
#define U_INTERFACE_P_CONTEXT_P_INTERFACE(pInterface) ((void *) (((char *) U_INTERFACE_P_VECTOR_TABLE_P_INTERFACE(pInterface)) +  \
                                                                 (pInterface)->sizeVectorTableBytes))

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** An interface.
 */
typedef struct uInterface_t {
    uInterfaceDeinit_t pInterfaceDeinit; /**< the pInterfaceDeinit
                                              function pointer, as
                                              passed to
                                              pUInterfaceCreate(). */
    size_t sizeVectorTableBytes; /**< the size of the vector table
                                      which will follow this structure. */
    size_t sizeContextBytes; /**< the size of the context that will
                                  follow the vector table. */
    int32_t version; /**< the interface version as passed to
                          pUInterfaceCreate().*/
} uInterface_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Create and initialise an interface.
uInterfaceTable_t *pUInterfaceCreate(size_t sizeVectorTableBytes,
                                     size_t sizeContextBytes,
                                     int32_t version,
                                     uInterfaceInit_t pInterfaceInit,
                                     void *pInitParam,
                                     uInterfaceDeinit_t pInterfaceDeinit)
{
    uInterfaceTable_t *pInterfaceTable = NULL;
    uInterface_t *pInterface = NULL;

    // Allocate memory for uInterface_t, plus the vector table
    // which will follow immediately after it, plus any context
    // memory which the caller has asked for
    pInterface = (uInterface_t *) pUPortMalloc(U_INTERFACE_SIZE_BYTES(sizeVectorTableBytes,
                                                                      sizeContextBytes));
    if (pInterface != NULL) {
        memset(pInterface, 0, U_INTERFACE_SIZE_BYTES(sizeVectorTableBytes,
                                                     sizeContextBytes));
        pInterfaceTable = U_INTERFACE_P_VECTOR_TABLE_P_INTERFACE(pInterface);
        // Store the size of the vector table so that we can later
        // work out where the context starts
        pInterface->sizeVectorTableBytes = sizeVectorTableBytes;
        // Store the size of the context so that we know when there
        // is none
        pInterface->sizeContextBytes = sizeContextBytes;
        // Store the version so that the user can retrieve it with uInterfaceVersion()
        pInterface->version = version;
        // Store pInterfaceDeinit for when we shut things down
        pInterface->pInterfaceDeinit = pInterfaceDeinit;
        if (pInterfaceInit != NULL) {
            pInterfaceInit(pInterfaceTable, pInitParam);
        }
    }

    return pInterfaceTable;
}

// Get the context pointer of an interface
void *pUInterfaceContext(uInterfaceTable_t pInterfaceTable)
{
    void *pContext = NULL;
    uInterface_t *pInterface = NULL;

    if (pInterfaceTable != NULL) {
        pInterface = U_INTERFACE_P_INTERFACE_P_VECTOR_TABLE(pInterfaceTable);
        if (pInterface->sizeContextBytes > 0) {
            pContext = U_INTERFACE_P_CONTEXT_P_VECTOR_TABLE(pInterfaceTable);
        }
    }

    return pContext;
}

// Get the interface version.
int32_t uInterfaceVersion(uInterfaceTable_t pInterfaceTable)
{
    int32_t version = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uInterface_t *pInterface;

    if (pInterfaceTable != NULL) {
        pInterface = U_INTERFACE_P_INTERFACE_P_VECTOR_TABLE(pInterfaceTable);
        version = pInterface->version;
    }

    return version;
}

// Delete an interface.
void uInterfaceDelete(uInterfaceTable_t pInterfaceTable)
{
    uInterface_t *pInterface = NULL;

    if (pInterfaceTable != NULL) {
        pInterface = U_INTERFACE_P_INTERFACE_P_VECTOR_TABLE(pInterfaceTable);
        if (pInterface->pInterfaceDeinit != NULL) {
            // Call pInterfaceDeinit()
            pInterface->pInterfaceDeinit(pInterfaceTable);
        }
    }

    // Free memory
    uPortFree(pInterface);
}

// End of file
