/*
 * Copyright 2019-2024 u-blox
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
 * @brief Implementation of the port I2C API for the Linux platform.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>
#include "pthread.h"  // threadId

#include "u_compiler.h" // U_ATOMIC_XXX() macros

#include "u_error_common.h"

#include "u_linked_list.h"

#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_i2c.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure for storing information on i2c writes without stop bit.
 */
typedef struct {
    pthread_t threadId;
    int32_t handle;
    uint16_t address;
    uint8_t *pPendingWriteData;
    size_t pendingWriteLength;
} i2cPendingDataInfo_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to ensure thread-safety.
 */
static uPortMutexHandle_t gMutex = NULL;

/** Root of linked list for pending no stop bit write data.
 */
static uLinkedList_t *gpI2cPendingDataList = NULL;

/** Variable to keep track of the number of I2C interfaces open.
 */
static volatile int32_t gResourceAllocCount = 0;

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Find possible pending data for the specified task, handle and address.
 *  Ignore address parameter if equal 0 (invalid i2c address).
 */
static i2cPendingDataInfo_t *findPendingData(pthread_t threadId,
                                             int32_t handle,
                                             uint16_t address)
{
    uLinkedList_t *p = gpI2cPendingDataList;
    while (p != NULL) {
        i2cPendingDataInfo_t *pI2cData = (i2cPendingDataInfo_t *)(p->p);
        if ((pI2cData->threadId == threadId) &&
            (pI2cData->handle == handle) &&
            ((address == 0) || (pI2cData->address == address))) {
            return pI2cData;
        }
        p = p->pNext;
    }
    return NULL;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise I2C handling.
int32_t uPortI2cInit()
{
    uErrorCode_t errorCode = U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCode = uPortMutexCreate(&gMutex);
    }
    return (int32_t)errorCode;
}

// Shutdown I2C handling.
void uPortI2cDeinit()
{
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);
        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

// Open an I2C instance.
int32_t uPortI2cOpen(int32_t i2c, int32_t pinSda, int32_t pinSdc,
                     bool controller)
{
    int32_t errorCodeOrHandle;
    if ((pinSda != -1) || (pinSdc != -1) || !controller) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
    if (gMutex == NULL) {
        return (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    }
    char devName[25];
    snprintf(devName, sizeof(devName), "/dev/i2c-%d", i2c);
    // Open the I2C bus
    errorCodeOrHandle = open(devName, O_RDWR);
    if (errorCodeOrHandle >= 0) {
        U_ATOMIC_INCREMENT(&gResourceAllocCount);
    }
    return errorCodeOrHandle;
}

// Adopt an I2C instance.
int32_t uPortI2cAdopt(int32_t i2c, bool controller)
{
    return uPortI2cOpen(i2c, -1, -1, controller);
}

// Close an I2C instance.
void uPortI2cClose(int32_t handle)
{
    if (gMutex != NULL) {
        // Remove possible pending data.
        i2cPendingDataInfo_t *p;
        while ((p = findPendingData(pthread_self(), handle, 0)) != NULL) {
            uPortFree(p->pPendingWriteData);
            uPortFree(p);
            uLinkedListRemove(&gpI2cPendingDataList, p);
        }
        close(handle);
        U_ATOMIC_DECREMENT(&gResourceAllocCount);
    }
}

// Close an I2C instance and attempt to recover the I2C bus.
int32_t uPortI2cCloseRecoverBus(int32_t handle)
{
    (void) handle;
    // Probably not possible to do in user mode.
    return (int32_t)U_ERROR_COMMON_NOT_SUPPORTED;
}

// Set the I2C clock frequency.
int32_t uPortI2cSetClock(int32_t handle, int32_t clockHertz)
{
    (void) handle;
    (void) clockHertz;
    // Not possible to do in user mode. Controlled by device tree.
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Get the I2C clock frequency.
int32_t uPortI2cGetClock(int32_t handle)
{
    (void) handle;
    // Not possible to do in user mode. Controlled by device tree.
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Set the timeout for I2C.
int32_t uPortI2cSetTimeout(int32_t handle, int32_t timeoutMs)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    if (gMutex != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        U_PORT_MUTEX_LOCK(gMutex);
        if (ioctl(handle, I2C_TIMEOUT, (unsigned long)(timeoutMs / 10)) >= 0) {
            errorCode = U_ERROR_COMMON_SUCCESS;
        }

        U_PORT_MUTEX_UNLOCK(gMutex);
    }
    return (int32_t)errorCode;
}

// Get the timeout for I2C.
int32_t uPortI2cGetTimeout(int32_t handle)
{
    (void) handle;
    // There is no standard Linux api to get the current i2c timeout
    // setting when in user mode.
    return (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
}

// Send and/or receive over the I2C interface as a controller.
int32_t uPortI2cControllerSendReceive(int32_t handle, uint16_t address,
                                      const char *pSend, size_t bytesToSend,
                                      char *pReceive, size_t bytesToReceive)
{
    if ((pSend == NULL) && (pReceive == NULL)) {
        return (int32_t)U_ERROR_COMMON_SUCCESS;
    }
    int32_t errorOrSize = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    if (gMutex != NULL) {
        errorOrSize = (int32_t)U_ERROR_COMMON_PLATFORM;
        U_PORT_MUTEX_LOCK(gMutex);
        if (ioctl(handle, I2C_SLAVE, address) >= 0) {
            i2cPendingDataInfo_t *pI2cData = findPendingData(pthread_self(), handle, address);
            if (pI2cData != NULL) {
                // Pending no-stop-bit write/read for this thread and i2c block-address.
                // Zero these structs to keep Valgrind happy when ioctl() is called
                struct i2c_rdwr_ioctl_data packets = {0};
                struct i2c_msg messages[2] = {0};
                messages[0].addr = address;
                messages[0].flags = 0;
                messages[0].buf = (unsigned char *)pI2cData->pPendingWriteData;
                messages[0].len = (unsigned short)pI2cData->pendingWriteLength;
                messages[1].addr = address;
                messages[1].flags = I2C_M_RD; // This avoids stop bit after the write
                messages[1].buf = (unsigned char *)pReceive;
                messages[1].len = (unsigned short)bytesToReceive;
                packets.msgs = messages;
                packets.nmsgs = 2;
                int bytesReceived = ioctl(handle, I2C_RDWR, &packets);
                if (bytesReceived == (int)bytesToReceive) {
                    errorOrSize = bytesReceived;
                }
                uPortFree(pI2cData->pPendingWriteData);
                uPortFree(pI2cData);
                uLinkedListRemove(&gpI2cPendingDataList, pI2cData);
            } else {
                // Plain write and read.
                bool ok = true;
                if (pSend != NULL) {
                    ok = write(handle, pSend, bytesToSend) == bytesToSend;
                    if (ok) {
                        errorOrSize = (int32_t)U_ERROR_COMMON_SUCCESS;
                    }
                }
                if (ok && (pReceive != NULL)) {
                    size_t bytesReceived = read(handle, pReceive, bytesToReceive);
                    if (bytesReceived == bytesToReceive) {
                        errorOrSize = (int32_t)bytesReceived;
                    }
                }
            }
        }
        U_PORT_MUTEX_UNLOCK(gMutex);
    }
    return errorOrSize;
}

// Perform a send over the I2C interface as a controller.
int32_t uPortI2cControllerSend(int32_t handle, uint16_t address,
                               const char *pSend, size_t bytesToSend,
                               bool noStop)
{
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    if (gMutex != NULL) {
        errorCode = U_ERROR_COMMON_PLATFORM;
        U_PORT_MUTEX_LOCK(gMutex);
        if (noStop) {
            // Must delay the write to next read in order to avoid the stop bit.
            // Save info about the data, thread, i2c block and i2c address in the
            // global list and use it in next call to uPortI2cControllerSendReceive.
            errorCode = U_ERROR_COMMON_NO_MEMORY;
            i2cPendingDataInfo_t *pInfo = pUPortMalloc(sizeof(i2cPendingDataInfo_t));
            if (pInfo != NULL) {
                uint8_t *pData = (uint8_t *)pUPortMalloc(bytesToSend);
                if (pData != NULL) {
                    memcpy(pData, pSend, bytesToSend);
                    pInfo->threadId = pthread_self();
                    pInfo->handle = handle;
                    pInfo->address = address;
                    pInfo->pPendingWriteData = pData;
                    pInfo->pendingWriteLength = bytesToSend;
                    uLinkedListAdd(&gpI2cPendingDataList, (void *)pInfo);
                    errorCode = U_ERROR_COMMON_SUCCESS;
                } else {
                    uPortFree(pInfo);
                }
            }
        } else {
            // Plain write will send stop bit.
            if (ioctl(handle, I2C_SLAVE, address) >= 0) {
                if (write(handle, pSend, bytesToSend) == bytesToSend) {
                    errorCode = U_ERROR_COMMON_SUCCESS;
                }
            }
        }
        U_PORT_MUTEX_UNLOCK(gMutex);
    }
    return (int32_t)errorCode;
}

// Get the number of I2C interfaces currently open.
int32_t uPortI2cResourceAllocCount()
{
    return U_ATOMIC_GET(&gResourceAllocCount);
}

// End of file
