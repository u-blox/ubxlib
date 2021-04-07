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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of the power API for GNSS.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_os.h"  // Required by u_gnss_private.h
#include "u_port_gpio.h"

#include "u_gnss_types.h"
#include "u_gnss_private.h"
#include "u_gnss_pwr.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC VARIABLES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Power a GNSS chip on.
int32_t uGnssPwrOn(int32_t gnssHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    // Message buffer for the 20-byte CFG-PRT message
    char message[20] = {0};

    if (gUGnssPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
            if (pInstance->pinGnssEn >= 0) {
                errorCode = (int32_t) U_ERROR_COMMON_PLATFORM;
                if (uPortGpioSet(pInstance->pinGnssEn, 0) == 0) {
                    // Wait a moment for the device to power up.
                    uPortTaskBlock(U_GNSS_POWER_UP_TIME_MILLISECONDS);
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    if (pInstance->pendingSwitchOffNmea) {
                        // Switch off NMEA if required using CFG-PRT
                        // First poll the GNSS chip for the current
                        // configuration for the port we are connected on
                        message[0] = (char) pInstance->portNumber;
                        if (uGnssPrivateSendReceiveUbxMessage(pInstance,
                                                              0x06, 0x00,
                                                              message, 1,
                                                              message,
                                                              sizeof(message)) == sizeof(message)) {
                            message[12] = 0x01; // ubx protocol only
                            message[14] = 0x01; // ubx protocol only
                            uGnssPrivateSendUbxMessage(pInstance,
                                                       0x06, 0x00,
                                                       message,
                                                       sizeof(message));
                        }
                        pInstance->pendingSwitchOffNmea = false;
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// Power a GNSS chip off.
int32_t uGnssPwrOff(int32_t gnssHandle)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uGnssPrivateInstance_t *pInstance;
    uint32_t message[2] = {0 /* endless */, 0 /* backup */};

    if (gUGnssPrivateMutex != NULL) {
        U_PORT_MUTEX_LOCK(gUGnssPrivateMutex);

        pInstance = pUGnssPrivateGetInstance(gnssHandle);
        if (pInstance != NULL) {
            // Set the GNSS into backup mode using the command RMX-LPREQ
            errorCode = uGnssPrivateSendUbxMessage(pInstance, 0x02, 0x41,
                                                   (char *) message, sizeof(message));
        }

        U_PORT_MUTEX_UNLOCK(gUGnssPrivateMutex);
    }

    return errorCode;
}

// End of file
