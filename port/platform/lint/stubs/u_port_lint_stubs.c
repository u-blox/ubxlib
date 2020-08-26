/*
 * Copyright 2020 u-blox Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** @file
 * @brief Stubs to allow GCC to lint to be run on the ubxlib code.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif
#include "stddef.h"    // size_t
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"
#include "u_error_common.h"
#include "u_port_debug.h"
#include "u_port.h"
#include "u_port_gpio.h"
#include "u_port_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * STUBS FOR PUBLIC FUNCTIONS: PORT LAYER
 * -------------------------------------------------------------- */

int32_t uPortPlatformStart(void (*pEntryPoint)(void *),
                           void *pParameter,
                           size_t stackSizeBytes,
                           int32_t priority)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortInit()
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

void uPortDeinit()
{
}

int64_t uPortGetTickTimeMs()
{
    return 0;
}

void uPortLogF(const char *pFormat, ...)
{
}

int32_t uPortGpioConfig(uPortGpioConfig_t *pConfig)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortGpioSet(int32_t pin, int32_t level)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortGpioGet(int32_t pin)
{
    return 0;
}

int32_t uPortTaskCreate(void (*pFunction)(void *),
                        const char *pName,
                        size_t stackSizeBytes,
                        void *pParameter,
                        int32_t priority,
                        uPortTaskHandle_t *pTaskHandle)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortTaskDelete(const uPortTaskHandle_t taskHandle)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

bool uPortTaskIsThis(const uPortTaskHandle_t taskHandle)
{
    return false;
}

void uPortTaskBlock(int32_t delayMs)
{
}

int32_t uPortTaskStackMinFree(const uPortTaskHandle_t taskHandle)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortQueueCreate(size_t queueLength,
                         size_t itemSizeBytes,
                         uPortQueueHandle_t *pQueueHandle)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortQueueDelete(const uPortQueueHandle_t queueHandle)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortQueueSend(const uPortQueueHandle_t queueHandle,
                       const void *pEventData)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortQueueSendIrq(const uPortQueueHandle_t queueHandle,
                          const void *pEventData)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortQueueReceive(const uPortQueueHandle_t queueHandle,
                          void *pEventData)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortQueueTryReceive(const uPortQueueHandle_t queueHandle,
                             int32_t waitMs, void *pEventData)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortMutexCreate(uPortMutexHandle_t *pMutexHandle)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortMutexDelete(const uPortMutexHandle_t mutexHandle)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortMutexLock(const uPortMutexHandle_t mutexHandle)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortMutexTryLock(const uPortMutexHandle_t mutexHandle,
                          int32_t delayMs)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortMutexUnlock(const uPortMutexHandle_t mutexHandle)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortUartInit(int32_t pinTx, int32_t pinRx,
                      int32_t pinCts, int32_t pinRts,
                      int32_t baudRate,
                      int32_t uart,
                      uPortQueueHandle_t *pUartQueue)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortUartDeinit(int32_t uart)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortUartEventSend(const uPortQueueHandle_t queueHandle,
                           int32_t sizeBytesOrError)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortUartEventReceive(const uPortQueueHandle_t queueHandle)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortUartEventTryReceive(const uPortQueueHandle_t queueHandle,
                                 int32_t waitMs)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortUartGetReceiveSize(int32_t uart)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortUartRead(int32_t uart, void *pBuffer,
                      size_t sizeBytes)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

int32_t uPortUartWrite(int32_t uart, const void *pBuffer,
                       size_t sizeBytes)
{
    return U_ERROR_COMMON_NOT_IMPLEMENTED;
}

bool uPortIsRtsFlowControlEnabled(int32_t uart)
{
    return false;
}

bool uPortIsCtsFlowControlEnabled(int32_t uart)
{
    return false;
}

#ifdef __cplusplus
}
#endif

// End of file
