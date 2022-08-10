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
#include "u_port_i2c.h"
#include "u_port_event_queue.h"
#include "u_port_crypto.h"

// From u_port.h.
int32_t uPortPlatformStart(void (*pEntryPoint)(void *),
                           void *pParameter,
                           size_t stackSizeBytes,
                           int32_t priority)
{
    (void) pEntryPoint;
    (void) pParameter;
    (void) stackSizeBytes;
    (void) priority;
    return 0;
}
int32_t uPortInit()
{
    return 0;
}
void uPortDeinit()
{
}
int32_t uPortGetTickTimeMs()
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
int32_t uPortEnterCritical()
{
    return 0;
}
void uPortExitCritical()
{
}

// From u_port_debug.h.
void uPortLogF(const char *pFormat, ...)
{
    (void) pFormat;
}

// From u_port_os.h
int32_t uPortTaskCreate(void (*pFunction)(void *),
                        const char *pName,
                        size_t stackSizeBytes,
                        void *pParameter,
                        int32_t priority,
                        uPortTaskHandle_t *pTaskHandle)
{
    (void) pFunction;
    (void) pName;
    (void) stackSizeBytes;
    (void) pParameter;
    (void) priority;
    (void) pTaskHandle;
    return 0;
}
int32_t uPortTaskDelete(const uPortTaskHandle_t taskHandle)
{
    (void) taskHandle;
    return 0;
}
bool uPortTaskIsThis(const uPortTaskHandle_t taskHandle)
{
    (void) taskHandle;
    return false;
}
void uPortTaskBlock(int32_t delayMs)
{
    (void) delayMs;
}
int32_t uPortTaskStackMinFree(const uPortTaskHandle_t taskHandle)
{
    (void) taskHandle;
    return 0;
}
int32_t uPortTaskGetHandle(uPortTaskHandle_t *pTaskHandle)
{
    (void) pTaskHandle;
    return 0;
}
int32_t uPortQueueCreate(size_t queueLength,
                         size_t itemSizeBytes,
                         uPortQueueHandle_t *pQueueHandle)
{
    (void) queueLength;
    (void) itemSizeBytes;
    (void) pQueueHandle;
    return 0;
}
int32_t uPortQueueDelete(const uPortQueueHandle_t queueHandle)
{
    (void) queueHandle;
    return 0;
}
int32_t uPortQueueSend(const uPortQueueHandle_t queueHandle,
                       const void *pEventData)
{
    (void) queueHandle;
    (void) pEventData;
    return 0;
}
int32_t uPortQueueSendIrq(const uPortQueueHandle_t queueHandle,
                          const void *pEventData)
{
    (void) queueHandle;
    (void) pEventData;
    return 0;
}
int32_t uPortQueueReceive(const uPortQueueHandle_t queueHandle,
                          void *pEventData)
{
    (void) queueHandle;
    (void) pEventData;
    return 0;
}
int32_t uPortQueueTryReceive(const uPortQueueHandle_t queueHandle,
                             int32_t waitMs, void *pEventData)
{
    (void) queueHandle;
    (void) waitMs;
    (void) pEventData;
    return 0;
}
int32_t uPortQueuePeek(const uPortQueueHandle_t queueHandle,
                       void *pEventData)
{
    (void) queueHandle;
    (void) pEventData;
    return 0;
}
int32_t uPortQueueGetFree(const uPortQueueHandle_t queueHandle)
{
    (void) queueHandle;
    return 0;
}
int32_t uPortMutexCreate(uPortMutexHandle_t *pMutexHandle)
{
    (void) pMutexHandle;
    return 0;
}
int32_t uPortMutexDelete(const uPortMutexHandle_t mutexHandle)
{
    (void) mutexHandle;
    return 0;
}
int32_t uPortMutexLock(const uPortMutexHandle_t mutexHandle)
{
    (void) mutexHandle;
    return 0;
}
int32_t uPortMutexTryLock(const uPortMutexHandle_t mutexHandle,
                          int32_t delayMs)
{
    (void) mutexHandle;
    (void) delayMs;
    return 0;
}
int32_t uPortMutexUnlock(const uPortMutexHandle_t mutexHandle)
{
    (void) mutexHandle;
    return 0;
}

int32_t uPortSemaphoreCreate(uPortSemaphoreHandle_t *pSemaphoreHandle,
                             uint32_t initialCount,
                             uint32_t limit)
{
    (void)pSemaphoreHandle;
    (void)initialCount;
    (void)limit;
    return 0;
}
int32_t uPortSemaphoreDelete(const uPortSemaphoreHandle_t semaphoreHandle)
{
    (void)semaphoreHandle;
    return 0;
}
int32_t uPortSemaphoreTake(const uPortSemaphoreHandle_t semaphoreHandle)
{
    (void)semaphoreHandle;
    return 0;
}
int32_t uPortSemaphoreTryTake(const uPortSemaphoreHandle_t semaphoreHandle,
                              int32_t delayMs)
{
    (void)semaphoreHandle;
    (void)delayMs;
    return 0;
}
int32_t uPortSemaphoreGive(const uPortSemaphoreHandle_t semaphoreHandle)
{
    (void)semaphoreHandle;
    return 0;
}
int32_t uPortSemaphoreGiveIrq(const uPortSemaphoreHandle_t semaphoreHandle)
{
    (void)semaphoreHandle;
    return 0;
}

int32_t uPortTimerCreate(uPortTimerHandle_t *pTimerHandle,
                         const char *pName,
                         pTimerCallback_t *pCallback,
                         void *pCallbackParam,
                         uint32_t intervalMs,
                         bool periodic)
{
    (void) pTimerHandle;
    (void) pName;
    (void) pCallback;
    (void) pCallbackParam;
    (void) intervalMs;
    (void) periodic;
    return 0;
}
int32_t uPortTimerDelete(const uPortTimerHandle_t timerHandle)
{
    (void) timerHandle;
    return 0;
}
int32_t uPortTimerStart(const uPortTimerHandle_t timerHandle)
{
    (void) timerHandle;
    return 0;
}
int32_t uPortTimerStop(const uPortTimerHandle_t timerHandle)
{
    (void) timerHandle;
    return 0;
}
int32_t uPortTimerChange(const uPortTimerHandle_t timerHandle,
                         uint32_t intervalMs)
{
    (void) timerHandle;
    (void) intervalMs;
    return 0;
}

void *uPortAcquireExecutableChunk(void *pChunkToMakeExecutable,
                                  size_t *pSize,
                                  uPortExeChunkFlags_t flags,
                                  uPortChunkIndex_t index)
{
    (void) pChunkToMakeExecutable;
    (void) pSize;
    (void) flags;
    (void) index;
    return NULL;
}

// From u_port_gpio.h
int32_t uPortGpioConfig(uPortGpioConfig_t *pConfig)
{
    (void) pConfig;
    return 0;
}
int32_t uPortGpioSet(int32_t pin, int32_t level)
{
    (void) pin;
    (void) level;
    return 0;
}
int32_t uPortGpioGet(int32_t pin)
{
    (void) pin;
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
    (void) uart;
    (void) baudRate;
    (void) pReceiveBuffer;
    (void) receiveBufferSizeBytes;
    (void) pinTx;
    (void) pinRx;
    (void) pinCts;
    (void) pinRts;
    return 0;
}
void uPortUartClose(int32_t handle)
{
    (void) handle;
}
int32_t uPortUartGetReceiveSize(int32_t handle)
{
    (void) handle;
    return 0;
}
int32_t uPortUartRead(int32_t handle, void *pBuffer,
                      size_t sizeBytes)
{
    (void) handle;
    (void) pBuffer;
    (void) sizeBytes;
    return 0;
}
int32_t uPortUartWrite(int32_t handle, const void *pBuffer,
                       size_t sizeBytes)
{
    (void) handle;
    (void) pBuffer;
    (void) sizeBytes;
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
    (void) handle;
    (void) filter;
    (void) pFunction;
    (void) pParam;
    (void) stackSizeBytes;
    (void) priority;
    return 0;
}
void uPortUartEventCallbackRemove(int32_t handle)
{
    (void) handle;
}
uint32_t uPortUartEventCallbackFilterGet(int32_t handle)
{
    (void) handle;
    return 0;
}
int32_t uPortUartEventCallbackFilterSet(int32_t handle,
                                        uint32_t filter)
{
    (void) handle;
    (void) filter;
    return 0;
}
int32_t uPortUartEventSend(int32_t handle, uint32_t eventBitMap)
{
    (void) handle;
    (void) eventBitMap;
    return 0;
}
int32_t uPortUartEventTrySend(int32_t handle, uint32_t eventBitMap,
                              int32_t delayMs)
{
    (void) handle;
    (void) eventBitMap;
    return 0;
}
bool uPortUartEventIsCallback(int32_t handle)
{
    (void) handle;
    return false;
}
int32_t uPortUartEventStackMinFree(int32_t handle)
{
    (void) handle;
    return 0;
}
bool uPortUartIsRtsFlowControlEnabled(int32_t handle)
{
    (void) handle;
    return false;
}
bool uPortUartIsCtsFlowControlEnabled(int32_t handle)
{
    (void) handle;
    return false;
}
int32_t uPortUartCtsSuspend(int32_t handle)
{
    (void) handle;
    return 0;
}
void uPortUartCtsResume(int32_t handle)
{
    (void) handle;
}

// From u_port_i2c.h
int32_t uPortI2cInit()
{
    return 0;
}
void uPortI2cDeinit()
{
}
int32_t uPortI2cOpen(int32_t i2c, int32_t pinSda, int32_t pinSdc,
                     bool controller)
{
    (void) i2c;
    (void) pinSda;
    (void) pinSdc;
    (void) controller;
    return 0;
}
int32_t uPortI2cAdopt(int32_t i2c, bool controller)
{
    (void) i2c;
    (void) controller;
    return 0;
}
void uPortI2cClose(int32_t handle)
{
    (void) handle;
}
int32_t uPortI2cCloseRecoverBus(int32_t handle)
{
    (void) handle;
    return 0;
}
int32_t uPortI2cSetClock(int32_t handle, int32_t clockHertz)
{
    (void) handle;
    (void) clockHertz;
    return 0;
}
int32_t uPortI2cGetClock(int32_t handle)
{
    (void) handle;
    return 0;
}
int32_t uPortI2cSetTimeout(int32_t handle, int32_t timeoutMs)
{
    (void) handle;
    (void) timeoutMs;
    return 0;
}
int32_t uPortI2cGetTimeout(int32_t handle)
{
    (void) handle;
    return 0;
}
int32_t uPortI2cControllerSendReceive(int32_t handle, uint16_t address,
                                      const char *pSend, size_t bytesToSend,
                                      char *pReceive, size_t bytesToReceive)
{
    (void) handle;
    (void) address;
    (void) pSend;
    (void) bytesToSend;
    (void) pReceive;
    (void) bytesToReceive;
    return 0;
}
int32_t uPortI2cControllerSend(int32_t handle, uint16_t address,
                               const char *pSend, size_t bytesToSend,
                               bool noStop)
{
    (void) handle;
    (void) address;
    (void) pSend;
    (void) bytesToSend;
    (void) noStop;
    return 0;
}

// From u_port_crypto.h.
int32_t uPortCryptoSha256(const char *pInput,
                          size_t inputLengthBytes,
                          char *pOutput)
{
    (void) pInput;
    (void) inputLengthBytes;
    (void) pOutput;
    return 0;
}
int32_t uPortCryptoHmacSha256(const char *pKey,
                              size_t keyLengthBytes,
                              const char *pInput,
                              size_t inputLengthBytes,
                              char *pOutput)
{
    (void) pKey;
    (void) keyLengthBytes;
    (void) pInput;
    (void) inputLengthBytes;
    (void) pOutput;
    return 0;
}
int32_t uPortCryptoAes128CbcEncrypt(const char *pKey,
                                    size_t keyLengthBytes,
                                    char *pInitVector,
                                    const char *pInput,
                                    size_t lengthBytes,
                                    char *pOutput)
{
    (void) pKey;
    (void) keyLengthBytes;
    (void) pInitVector;
    (void) pInput;
    (void) lengthBytes;
    (void) pOutput;
    return 0;
}
int32_t uPortCryptoAes128CbcDecrypt(const char *pKey,
                                    size_t keyLengthBytes,
                                    char *pInitVector,
                                    const char *pInput,
                                    size_t lengthBytes,
                                    char *pOutput)
{
    (void) pKey;
    (void) keyLengthBytes;
    (void) pInitVector;
    (void) pInput;
    (void) lengthBytes;
    (void) pOutput;
    return 0;
}

// End of file
