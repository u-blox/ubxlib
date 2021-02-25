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
 * @brief Stubs for porting files.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "u_port.h"
#include "u_port_debug.h"
#include "u_port_os.h"
#include "u_port_gpio.h"
#include "u_port_uart.h"
#include "u_port_event_queue.h"
#include "u_port_crypto.h"

// From u_port.h.
int32_t uPortPlatformStart(void (*pEntryPoint)(void *),
                           void *pParameter,
                           size_t stackSizeBytes,
                           int32_t priority)
{
    return 0;
}
int32_t uPortInit()
{
    return 0;
}
void uPortDeinit()
{
}
int64_t uPortGetTickTimeMs()
{
    return 0;
}
int32_t uPortGetHeapMinFree()
{
    return 0;
}
int32_t uPortGetHeapFree()
{
    return 0;
}

// From u_port_debug.h.
void uPortLogF(const char *pFormat, ...)
{
}

// From u_port_os.h
int32_t uPortTaskCreate(void (*pFunction)(void *),
                        const char *pName,
                        size_t stackSizeBytes,
                        void *pParameter,
                        int32_t priority,
                        uPortTaskHandle_t *pTaskHandle)
{
    return 0;
}
int32_t uPortTaskDelete(const uPortTaskHandle_t taskHandle)
{
    return 0;
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
    return 0;
}
int32_t uPortQueueCreate(size_t queueLength,
                         size_t itemSizeBytes,
                         uPortQueueHandle_t *pQueueHandle)
{
    return 0;
}
int32_t uPortQueueDelete(const uPortQueueHandle_t queueHandle)
{
    return 0;
}
int32_t uPortQueueSend(const uPortQueueHandle_t queueHandle,
                       const void *pEventData)
{
    return 0;
}
int32_t uPortQueueSendIrq(const uPortQueueHandle_t queueHandle,
                          const void *pEventData)
{
    return 0;
}
int32_t uPortQueueReceive(const uPortQueueHandle_t queueHandle,
                          void *pEventData)
{
    return 0;
}
int32_t uPortQueueTryReceive(const uPortQueueHandle_t queueHandle,
                             int32_t waitMs, void *pEventData)
{
    return 0;
}
int32_t uPortMutexCreate(uPortMutexHandle_t *pMutexHandle)
{
    return 0;
}

int32_t uPortMutexDelete(const uPortMutexHandle_t mutexHandle)
{
    return 0;
}
int32_t uPortMutexLock(const uPortMutexHandle_t mutexHandle)
{
    return 0;
}
int32_t uPortMutexTryLock(const uPortMutexHandle_t mutexHandle,
                          int32_t delayMs)
{
    return 0;
}
int32_t uPortMutexUnlock(const uPortMutexHandle_t mutexHandle)
{
    return 0;
}
void *uPortAcquireExecutableChunk(void *pChunkToMakeExecutable,
                                  size_t *pSize,
                                  uPortExeChunkFlags_t flags,
                                  uPortChunkIndex_t index)
{
}

// From u_port_gpio.h
int32_t uPortGpioConfig(uPortGpioConfig_t *pConfig)
{
    return 0;
}
int32_t uPortGpioSet(int32_t pin, int32_t level)
{
    return 0;
}
int32_t uPortGpioGet(int32_t pin)
{
    return 0;
}

// From u_port_uart.h
int32_t uPortUartInit()
{
    return 0;
}
void uPortUartDeinit()
{
}
int32_t uPortUartOpen(int32_t uart, int32_t baudRate,
                      void *pReceiveBuffer,
                      size_t receiveBufferSizeBytes,
                      int32_t pinTx, int32_t pinRx,
                      int32_t pinCts, int32_t pinRts)
{
    return 0;
}
void uPortUartClose(int32_t handle)
{
}
int32_t uPortUartGetReceiveSize(int32_t handle)
{
    return 0;
}
int32_t uPortUartRead(int32_t handle, void *pBuffer,
                      size_t sizeBytes)
{
    return 0;
}
int32_t uPortUartWrite(int32_t handle, const void *pBuffer,
                       size_t sizeBytes)
{
    return 0;
}
int32_t uPortUartEventCallbackSet(int32_t handle,
                                  uint32_t filter,
                                  void (*pFunction)(int32_t, uint32_t,
                                                    void *),
                                  void *pParam,
                                  size_t stackSizeBytes,
                                  int32_t priority)
{
    return 0;
}
void uPortUartEventCallbackRemove(int32_t handle)
{
}
uint32_t uPortUartEventCallbackFilterGet(int32_t handle)
{
    return 0;
}
int32_t uPortUartEventCallbackFilterSet(int32_t handle,
                                        uint32_t filter)
{
    return 0;
}
int32_t uPortUartEventSend(int32_t handle, uint32_t eventBitMap)
{
    return 0;
}
bool uPortUartEventIsCallback(int32_t handle)
{
    return false;
}
int32_t uPortUartEventStackMinFree(int32_t handle)
{
    return 0;
}
bool uPortUartIsRtsFlowControlEnabled(int32_t handle)
{
    return false;
}
bool uPortUartIsCtsFlowControlEnabled(int32_t handle)
{
    return false;
}

// From u_port_crypto.h.
int32_t uPortCryptoSha256(const char *pInput,
                          size_t inputLengthBytes,
                          char *pOutput)
{
    return 0;
}
int32_t uPortCryptoHmacSha256(const char *pKey,
                              size_t keyLengthBytes,
                              const char *pInput,
                              size_t inputLengthBytes,
                              char *pOutput)
{
    return 0;
}
int32_t uPortCryptoAes128CbcEncrypt(const char *pKey,
                                    size_t keyLengthBytes,
                                    char *pInitVector,
                                    const char *pInput,
                                    size_t lengthBytes,
                                    char *pOutput)
{
    return 0;
}
int32_t uPortCryptoAes128CbcDecrypt(const char *pKey,
                                    size_t keyLengthBytes,
                                    char *pInitVector,
                                    const char *pInput,
                                    size_t lengthBytes,
                                    char *pOutput)
{
    return 0;
}

// End of file
