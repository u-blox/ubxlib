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
 * @brief Implementation of the port SPI API for the Linux platform.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <linux/spi/spidev.h>

#include "u_error_common.h"
#include "u_port.h"
#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_common_spi.h"

#include "u_port_spi.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

#ifndef U_PORT_SPI_MAX_NUM
/** The maximum number of SPI HW blocks that are available;.
 */
# define U_PORT_SPI_MAX_NUM 2
#endif

#define IS_VALID_HANDLE(h) ((h >= 0) && (h < U_PORT_SPI_MAX_NUM))
#define VALIDATE_HANDLE(h)                            \
  if (!IS_VALID_HANDLE(h)) {                              \
    return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER; \
  }

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure of the things we need to keep track of per SPI interface.
 *  Please note that the ubxlib api currently only allow one device
 *  per SPI block.
 */
typedef struct {
    int fd;
    uCommonSpiControllerDevice_t devCfg;
} uPortSpiCfg_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Mutex to ensure thread-safety.
 */
static uPortMutexHandle_t gMutex = NULL;

/** SPI configuration data.
 */
static uPortSpiCfg_t gSpiCfg[U_PORT_SPI_MAX_NUM];

static uCommonSpiControllerDevice_t gDefaultDevCfg =
    U_COMMON_SPI_CONTROLLER_DEVICE_INDEX_DEFAULTS(0);

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/** Open a character file handle for a SPI device.
*/
static bool checkOpenDevice(int32_t handle)
{
    if (gSpiCfg[handle].fd == -1) {
        // Not opened before
        char devName[25];
        // The index is used as device selection and the corresponding
        // cs pin is determined by the device tree.
        snprintf(devName, sizeof(devName),
                 "/dev/spidev%d.%d",
                 handle, gSpiCfg[handle].devCfg.indexSelect);
        gSpiCfg[handle].fd = open(devName, O_RDWR);
    }
    return (gSpiCfg[handle].fd != -1);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise SPI handling.
int32_t uPortSpiInit()
{
    uErrorCode_t errorCode = U_ERROR_COMMON_SUCCESS;

    if (gMutex == NULL) {
        errorCode = uPortMutexCreate(&gMutex);
        for (int32_t i = 0; i < U_PORT_SPI_MAX_NUM; i++) {
            gSpiCfg[i].fd = -1;
            gSpiCfg[i].devCfg = gDefaultDevCfg;
        }
    }
    return (int32_t)errorCode;
}

// Shutdown SPI handling.
void uPortSpiDeinit()
{
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);
        U_PORT_MUTEX_UNLOCK(gMutex);
        uPortMutexDelete(gMutex);
        gMutex = NULL;
    }
}

// Open an SPI instance.
int32_t uPortSpiOpen(int32_t spi, int32_t pinMosi, int32_t pinMiso,
                     int32_t pinClk, bool controller)
{
    if (gMutex == NULL) {
        return (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    }
    if ((pinMosi != -1) ||
        (pinMiso != -1) ||
        (pinClk != -1) ||
        !IS_VALID_HANDLE(spi) ||
        !controller) {
        return (int32_t)U_ERROR_COMMON_INVALID_PARAMETER;
    }
    // Use the block number as handle.
    return spi;
}

// Close an SPI instance.
void uPortSpiClose(int32_t handle)
{
    if ((gMutex != NULL) && IS_VALID_HANDLE(handle)) {
        U_PORT_MUTEX_LOCK(gMutex);
        if (gSpiCfg[handle].fd != -1) {
            close(gSpiCfg[handle].fd);
            gSpiCfg[handle].fd = -1;
        }
        gSpiCfg[handle].devCfg = gDefaultDevCfg;
        U_PORT_MUTEX_UNLOCK(gMutex);
    }
}

// Set the configuration of the device.
int32_t uPortSpiControllerSetDevice(int32_t handle,
                                    const uCommonSpiControllerDevice_t *pDevice)
{
    VALIDATE_HANDLE(handle);
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);
        if (checkOpenDevice(handle)) {
            errorCode = U_ERROR_COMMON_SUCCESS;
            // Set the possible configuration settings.
            gSpiCfg[handle].devCfg = *pDevice;
            int fd = gSpiCfg[handle].fd;
            if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &(pDevice->frequencyHertz)) == -1) {
                errorCode = U_ERROR_COMMON_PLATFORM;
            }
            uint8_t mode = (uint8_t)(pDevice->mode);
            if (ioctl(fd, SPI_IOC_WR_MODE, &mode) == -1) {
                errorCode = U_ERROR_COMMON_PLATFORM;
            }
            uint8_t bits = (uint8_t)(pDevice->wordSizeBytes * 8);
            if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) == -1) {
                errorCode = U_ERROR_COMMON_PLATFORM;
            }
            uint8_t lsbFirst = pDevice->lsbFirst ? 1 : 0;
            if (ioctl(fd, SPI_IOC_WR_LSB_FIRST, &lsbFirst) == -1) {
                errorCode = U_ERROR_COMMON_PLATFORM;
            }
        } else {
            errorCode = U_ERROR_COMMON_PLATFORM;
        }
        U_PORT_MUTEX_UNLOCK(gMutex);
    }
    return (int32_t) errorCode;
}

// Get the configuration of the device.
int32_t uPortSpiControllerGetDevice(int32_t handle,
                                    uCommonSpiControllerDevice_t *pDevice)
{
    VALIDATE_HANDLE(handle);
    if (pDevice == NULL) {
        return U_ERROR_COMMON_INVALID_PARAMETER;
    }
    uErrorCode_t errorCode = U_ERROR_COMMON_NOT_INITIALISED;
    if (gMutex != NULL) {
        U_PORT_MUTEX_LOCK(gMutex);
        if (checkOpenDevice(handle)) {
            errorCode = U_ERROR_COMMON_SUCCESS;
            // Get the settings possible to retrieve.
            *pDevice = gDefaultDevCfg;
            int fd = gSpiCfg[handle].fd;
            if (ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &(pDevice->frequencyHertz)) == -1) {
                errorCode = U_ERROR_COMMON_PLATFORM;
            }

            uint8_t mode;
            if (ioctl(fd, SPI_IOC_WR_MODE, &mode) == -1) {
                errorCode = U_ERROR_COMMON_PLATFORM;
            }
            pDevice->mode = (uCommonSpiMode_t)mode;

            uint8_t bits;
            if (ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits) == -1) {
                errorCode = U_ERROR_COMMON_PLATFORM;
            }
            pDevice->wordSizeBytes = bits / 8;

            uint8_t lsbFirst;
            if (ioctl(fd, SPI_IOC_RD_LSB_FIRST, &lsbFirst) == -1) {
                errorCode = U_ERROR_COMMON_PLATFORM;
            }
            pDevice->lsbFirst = lsbFirst == 1;
        } else {
            errorCode = U_ERROR_COMMON_PLATFORM;
        }
        U_PORT_MUTEX_UNLOCK(gMutex);
    }
    return (int32_t)errorCode;
}

// Exchange a single word with an SPI device.
uint64_t uPortSpiControllerSendReceiveWord(int32_t handle, uint64_t value,
                                           size_t bytesToSendAndReceive)
{
    uint64_t valueReceived = 0;
    if ((gMutex != NULL) && IS_VALID_HANDLE(handle)) {
        // Need to perform byte reversal if the length of the word we
        // are sending is greater than one byte, there is a mismatch between
        // the endianness of this processor and the endianness of
        // bit-transmission, and it will only work if the word size is set
        // to eight bits
        bool reverseBytes = ((bytesToSendAndReceive > 1) &&
                             (gSpiCfg[handle].devCfg.lsbFirst !=
                              U_PORT_IS_LITTLE_ENDIAN) &&
                             (gSpiCfg[handle].devCfg.wordSizeBytes == 1));
        if (reverseBytes) {
            U_PORT_BYTE_REVERSE(value, bytesToSendAndReceive);
        }
        uPortSpiControllerSendReceiveBlock(handle,
                                           (const char *)&value,
                                           bytesToSendAndReceive,
                                           (char *)&valueReceived,
                                           bytesToSendAndReceive);
        if (reverseBytes) {
            U_PORT_BYTE_REVERSE(valueReceived, bytesToSendAndReceive);
        }
    }
    return valueReceived;
}

// Exchange a block of data with an SPI device.
int32_t uPortSpiControllerSendReceiveBlock(int32_t handle, const char *pSend,
                                           size_t bytesToSend, char *pReceive,
                                           size_t bytesToReceive)
{
    VALIDATE_HANDLE(handle);
    int32_t errorCodeOrReceiveSize = (int32_t)U_ERROR_COMMON_NOT_INITIALISED;
    size_t len = bytesToSend;
    if (gMutex != NULL) {
        char *pInBuff = NULL;
        char *pOutBuff = NULL;
        if (bytesToReceive < bytesToSend) {
            // Use temporary input buffer as pReceive is too small for the
            // bi-directional spi transaction.
            pInBuff = (char *)pUPortMalloc(bytesToSend);
            if (pInBuff == NULL) {
                return (int32_t)U_ERROR_COMMON_NO_MEMORY;
            }
        } else if (bytesToSend < bytesToReceive) {
            // Use temporary output buffer as pSend is too small for the
            // bi-directional spi transaction.
            pOutBuff = (char *)pUPortMalloc(bytesToReceive);
            if (pOutBuff != NULL) {
                // Copy the data we want to send into the start of the buffer
                memcpy(pOutBuff, pSend, bytesToSend);
                // Fill the remainder with 0xFF
                memset(pOutBuff + bytesToSend, 0xFF, bytesToReceive - bytesToSend);
                len = bytesToReceive;
            } else {
                return (int32_t)U_ERROR_COMMON_NO_MEMORY;
            }
        }
        U_PORT_MUTEX_LOCK(gMutex);
        errorCodeOrReceiveSize = (int32_t)U_ERROR_COMMON_PLATFORM;
        if (checkOpenDevice(handle)) {
            struct spi_ioc_transfer transf = {0};
            transf.tx_buf = (__u64)(pOutBuff ? pOutBuff : pSend);
            transf.rx_buf = (__u64)(pInBuff ? pInBuff : pReceive);
            transf.len = (unsigned int)len;
            transf.speed_hz = (unsigned int)gSpiCfg[handle].devCfg.frequencyHertz;
            transf.bits_per_word = (unsigned char)(gSpiCfg[handle].devCfg.wordSizeBytes * 8);
            if (ioctl(gSpiCfg[handle].fd, SPI_IOC_MESSAGE(1), &transf) != -1) {
                if (pInBuff != NULL) {
                    memcpy(pReceive, pInBuff, bytesToReceive);
                }
                errorCodeOrReceiveSize = bytesToReceive;
            }
        }
        uPortFree(pInBuff);
        uPortFree(pOutBuff);
        U_PORT_MUTEX_UNLOCK(gMutex);
    }
    return errorCodeOrReceiveSize;
}
// End of file
