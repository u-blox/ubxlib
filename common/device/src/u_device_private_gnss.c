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
 * @brief Functions associated with a GNSS device.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h"  // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"
#include "string.h"    // for memset()

#include "u_cfg_os_platform_specific.h" // For U_CFG_OS_CLIB_LEAKS

#include "u_error_common.h"

#include "u_port_os.h"
#include "u_port_heap.h"
#include "u_port_uart.h"
#include "u_port_i2c.h"
#include "u_port_spi.h"

#include "u_device.h"
#include "u_device_shared.h"
#include "u_device_serial.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell.h"                 // For uCellAtClientHandleGet()
#include "u_cell_loc.h"             // For uCellLocSetPinGnssPwr()/uCellLocSetPinGnssDataReady()

#include "u_short_range_module_type.h"
#include "u_short_range.h"          // For uShortRangeAtClientHandleGet()

#include "u_gnss_module_type.h"
#include "u_gnss_type.h"
#include "u_gnss.h"
#include "u_gnss_pwr.h"
#include "u_gnss_cfg.h" // For uGnssCfgSetProtocolOut()

#include "u_device_private.h"
#include "u_device_shared_gnss.h"
#include "u_device_private_gnss.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/** Table to convert device transport type to GNSS transport type.
 */
const uGnssTransportType_t gDeviceToGnssTransportType[] = {
    U_GNSS_TRANSPORT_NONE, // U_DEVICE_TRANSPORT_TYPE_NONE,
    U_GNSS_TRANSPORT_UART, // U_DEVICE_TRANSPORT_TYPE_UART or U_DEVICE_TRANSPORT_TYPE_UART_1,
    U_GNSS_TRANSPORT_I2C,  // U_DEVICE_TRANSPORT_TYPE_I2C,
    U_GNSS_TRANSPORT_SPI,  // U_DEVICE_TRANSPORT_TYPE_SPI,
    U_GNSS_TRANSPORT_VIRTUAL_SERIAL, // U_DEVICE_TRANSPORT_TYPE_VIRTUAL_SERIAL
    U_GNSS_TRANSPORT_UART_2 // U_DEVICE_TRANSPORT_TYPE_UART_2,
};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Populate the GNSS device context
static void populateContext(uDeviceGnssInstance_t *pContext,
                            uGnssTransportHandle_t gnssTransportHandle,
                            uDeviceTransportType_t deviceTransportType)
{
    switch (deviceTransportType) {
        case U_DEVICE_TRANSPORT_TYPE_UART:
        // fall-through
        case U_DEVICE_TRANSPORT_TYPE_UART_2:
            pContext->transportHandle.int32Handle = gnssTransportHandle.uart;
            break;
        case U_DEVICE_TRANSPORT_TYPE_I2C:
            pContext->transportHandle.int32Handle = gnssTransportHandle.i2c;
            break;
        case U_DEVICE_TRANSPORT_TYPE_SPI:
            pContext->transportHandle.int32Handle = gnssTransportHandle.spi;
            break;
        case U_DEVICE_TRANSPORT_TYPE_VIRTUAL_SERIAL:
            pContext->transportHandle.pDeviceSerial = gnssTransportHandle.pDeviceSerial;
            break;
        default:
            break;
    }
    pContext->deviceTransportType = deviceTransportType;
}

// Do all the leg-work to remove a GNSS device.
static int32_t removeDevice(uDeviceHandle_t devHandle, bool powerOff)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uDeviceGnssInstance_t *pContext = (uDeviceGnssInstance_t *) U_DEVICE_INSTANCE(devHandle)->pContext;

    if (pContext != NULL) {
        errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
        if (powerOff) {
            errorCode = uGnssPwrOff(devHandle);
            if (errorCode == 0) {
                // This will destroy the instance
                uGnssRemove(devHandle);
                uPortFree(pContext);
            }
        } else {
            uPortFree(pContext);
        }
    }

    return errorCode;
}

// Do all the leg-work to add a GNSS device.
static int32_t addDevice(uGnssTransportHandle_t gnssTransportHandle,
                         uDeviceTransportType_t deviceTransportType,
                         const uDeviceCfgGnss_t *pCfgGnss,
                         uDeviceHandle_t *pDeviceHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
    uGnssTransportType_t gnssTransportType = U_GNSS_TRANSPORT_NONE;
    uDeviceGnssInstance_t *pContext;

    // Populate gnssTransportType
    if ((deviceTransportType >= 0) &&
        (deviceTransportType < sizeof(gDeviceToGnssTransportType) / sizeof(
             gDeviceToGnssTransportType[0]))) {
        gnssTransportType = gDeviceToGnssTransportType[deviceTransportType];
    }

    pContext = (uDeviceGnssInstance_t *) pUPortMalloc(sizeof(uDeviceGnssInstance_t));
    if (pContext != NULL) {
        populateContext(pContext, gnssTransportHandle, deviceTransportType);
        // Add the GNSS instance, which actually creates pDeviceHandle
        errorCode = uGnssAdd((uGnssModuleType_t) pCfgGnss->moduleType,
                             gnssTransportType, gnssTransportHandle,
                             pCfgGnss->pinEnablePower, false,
                             pDeviceHandle);
        if (errorCode == 0) {
            if (pCfgGnss->i2cAddress > 0) {
                uGnssSetI2cAddress(*pDeviceHandle, pCfgGnss->i2cAddress);
            }
#if !U_CFG_OS_CLIB_LEAKS
            // Set printing of commands sent to the GNSS chip,
            // which can be useful while debugging, but
            // only if the C library doesn't leak.
            uGnssSetUbxMessagePrint(*pDeviceHandle, true);
#endif
            // Attach the context
            U_DEVICE_INSTANCE(*pDeviceHandle)->pContext = pContext;
            // Power on the GNSS chip
            errorCode = uGnssPwrOn(*pDeviceHandle);
            if (errorCode != 0) {
                // If we failed to power on, clean up
                removeDevice(*pDeviceHandle, false);
            }
        } else {
            uPortFree(pContext);
        }
    }

    return errorCode;
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS: WORKAROUND FOR LINKER ISSUE
 * -------------------------------------------------------------- */

void uDevicePrivateGnssLink()
{
    //dummy
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Initialise GNSS.
int32_t uDevicePrivateGnssInit()
{
    return uGnssInit();
}

// Deinitialise GNSS.
void uDevicePrivateGnssDeinit()
{
    uGnssDeinit();
}

// Power up a GNSS device, making it available for configuration.
int32_t uDevicePrivateGnssAdd(const uDeviceCfg_t *pDevCfg,
                              uDeviceHandle_t *pDeviceHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uGnssTransportHandle_t gnssTransportHandle;
    int32_t x;
    const uDeviceCfgUart_t *pCfgUart;
    const uDeviceCfgI2c_t *pCfgI2c;
    const uDeviceCfgSpi_t *pCfgSpi;
    const uDeviceCfgVirtualSerial_t *pCfgVirtualSerial;
    const uDeviceCfgGnss_t *pCfgGnss;
    uDeviceSerial_t *pDeviceSerial;

    if ((pDevCfg != NULL) && (pDeviceHandle != NULL)) {
        pCfgGnss = &(pDevCfg->deviceCfg.cfgGnss);
        if (pCfgGnss->version == 0) {
            switch (pDevCfg->transportType) {
                case U_DEVICE_TRANSPORT_TYPE_UART:
                // fall-through
                case U_DEVICE_TRANSPORT_TYPE_UART_2:
                    pCfgUart = &(pDevCfg->transportCfg.cfgUart);
                    if (pCfgUart->pPrefix != NULL) {
                        uPortUartPrefix(pCfgUart->pPrefix);
                    }
                    // Open a UART with the recommended buffer length
                    // and default baud rate.
                    errorCode = uPortUartOpen(pCfgUart->uart,
                                              pCfgUart->baudRate, NULL,
                                              U_GNSS_UART_BUFFER_LENGTH_BYTES,
                                              pCfgUart->pinTxd,
                                              pCfgUart->pinRxd,
                                              pCfgUart->pinCts,
                                              pCfgUart->pinRts);
                    if (errorCode >= 0) {
                        gnssTransportHandle.uart = errorCode;
                        errorCode = addDevice(gnssTransportHandle,
                                              pDevCfg->transportType,
                                              pCfgGnss, pDeviceHandle);
                        if (errorCode < 0) {
                            // Clean up on error
                            uPortUartClose(gnssTransportHandle.uart);
                        }
                    }
                    break;
                case U_DEVICE_TRANSPORT_TYPE_I2C:
                    pCfgI2c = &(pDevCfg->transportCfg.cfgI2c);
                    // Open the I2C instance.
                    errorCode = uDevicePrivateI2cOpen(pCfgI2c);
                    if (errorCode >= 0) {
                        gnssTransportHandle.i2c = errorCode;
                        errorCode = addDevice(gnssTransportHandle,
                                              pDevCfg->transportType,
                                              pCfgGnss, pDeviceHandle);
                        if (errorCode >= 0) {
                            // Log that the device is using the given I2C HW
                            x = uDevicePrivateI2cIsUsedBy(*pDeviceHandle, pCfgI2c);
                            if (x < 0) {
                                errorCode = x;
                                // Clean up if there's no room
                                removeDevice(*pDeviceHandle, true);
                            }
                        } else {
                            // Clean up on error
                            uDevicePrivateI2cCloseCfgI2c(pCfgI2c);
                        }
                    }
                    break;
                case U_DEVICE_TRANSPORT_TYPE_SPI:
                    pCfgSpi = &(pDevCfg->transportCfg.cfgSpi);
                    // Open SPI.
                    errorCode = uPortSpiOpen(pCfgSpi->spi,
                                             pCfgSpi->pinMosi,
                                             pCfgSpi->pinMiso,
                                             pCfgSpi->pinClk,
                                             true);
                    if (errorCode >= 0) {
                        gnssTransportHandle.spi = errorCode;
                        errorCode = uPortSpiControllerSetDevice(errorCode,
                                                                &(pCfgSpi->device));
                        if (errorCode == 0) {
                            errorCode = addDevice(gnssTransportHandle,
                                                  pDevCfg->transportType,
                                                  pCfgGnss, pDeviceHandle);
                        }
                        if (errorCode < 0) {
                            // Clean up on error
                            uPortSpiClose(gnssTransportHandle.spi);
                        }
                    }
                    break;
                case U_DEVICE_TRANSPORT_TYPE_VIRTUAL_SERIAL:
                    pCfgVirtualSerial = &(pDevCfg->transportCfg.cfgVirtualSerial);
                    pDeviceSerial = pCfgVirtualSerial->pDevice;
                    // Open a virtual serial port with the recommended buffer length
                    errorCode = pDeviceSerial->open(pDeviceSerial, NULL,
                                                    U_GNSS_UART_BUFFER_LENGTH_BYTES);
                    if (errorCode == 0) {
                        gnssTransportHandle.pDeviceSerial = pDeviceSerial;
                        errorCode = addDevice(gnssTransportHandle,
                                              pDevCfg->transportType,
                                              pCfgGnss, pDeviceHandle);
                        if (errorCode < 0) {
                            // Clean up on error
                            pDeviceSerial->close(pDeviceSerial);
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }

    return errorCode;
}

// Remove a GNSS device.
int32_t uDevicePrivateGnssRemove(uDeviceHandle_t devHandle,
                                 bool powerOff)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
    uDeviceGnssInstance_t *pContext = (uDeviceGnssInstance_t *) U_DEVICE_INSTANCE(devHandle)->pContext;

    if (pContext != NULL) {
        errorCode = removeDevice(devHandle, powerOff);
        if (errorCode == 0) {
            // Having removed the device, close the transport
            switch (pContext->deviceTransportType) {
                case U_DEVICE_TRANSPORT_TYPE_UART:
                // fall-through
                case U_DEVICE_TRANSPORT_TYPE_UART_2:
                    uPortUartClose(pContext->transportHandle.int32Handle);
                    break;
                case U_DEVICE_TRANSPORT_TYPE_I2C:
                    uDevicePrivateI2cCloseDevHandle(devHandle);
                    break;
                case U_DEVICE_TRANSPORT_TYPE_SPI:
                    uPortSpiClose(pContext->transportHandle.int32Handle);
                    break;
                case U_DEVICE_TRANSPORT_TYPE_VIRTUAL_SERIAL:
                    pContext->transportHandle.pDeviceSerial->close(pContext->transportHandle.pDeviceSerial);;
                    break;
                default:
                    break;
            }
        }
    }

    return errorCode;
}

// End of file
