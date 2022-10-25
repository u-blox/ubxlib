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

/* Only #includes of u_* and the C standard library are allowed here,
 * no platform stuff and no OS stuff.  Anything required from
 * the platform/OS must be brought in through u_port* to maintain
 * portability.
 */

/** @file
 * @brief Implementation of the FOTA API for cellular.
 */

#ifdef U_CFG_OVERRIDE
# include "u_cfg_override.h" // For a customer's configuration override
#endif

#include "stdlib.h"    // atoi()
#include "stddef.h"    // NULL, size_t etc.
#include "stdint.h"    // int32_t etc.
#include "stdbool.h"

#include "u_cfg_sw.h"

#include "u_error_common.h"

#include "u_port.h"
#include "u_port_heap.h"
#include "u_port_os.h"

#include "u_at_client.h"

#include "u_cell_module_type.h"
#include "u_cell_file.h"
#include "u_cell_net.h"     // Order is important here
#include "u_cell_private.h" // don't change it
#include "u_cell_fota.h"

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Structure to help translate the fail cases in the detailed
 * download status from the AT interface into our download status.
 */
typedef struct {
    int32_t atInterfaceStatus;
    uCellFotaStatusDownload_t status;
} uCellFotaStatusDownloadFailConvert_t;

/** Structure to help translate the codes returned by tje +UUFWINSTALL
 * URC into our install status.
 */
typedef struct {
    int32_t uufwInstallStatus;
    uCellFotaStatusInstall_t status;
} uCellFotaStatusInstallConvert_t;

/** All the parameters for the FOTA status callback.
 */
typedef struct {
    uDeviceHandle_t cellHandle;
    uCellFotaStatus_t status;
    uCellFotaStatusCallback_t *pCallback;
    void *pCallbackParam;
} uCellFotaStatusCallbackParameters_t;

/** Structure defining the FOTA context.
 */
typedef struct {
    uCellFotaStatusCallback_t *pCallback;
    void *pCallbackParameter;
} uCellPrivateFotaContext_t;

/* ----------------------------------------------------------------
 * VARIABLES
 * -------------------------------------------------------------- */

/** Array to convert the fail cases of the detailed download status
 * from the AT interface into our download status enum.
 */
static uCellFotaStatusDownloadFailConvert_t gAtDownloadFailureStatus[] = {
    {100, U_CELL_FOTA_STATUS_DOWNLOAD_USER_CANCEL},
    {101, U_CELL_FOTA_STATUS_DOWNLOAD_MEMORY_ERROR},
    {102, U_CELL_FOTA_STATUS_DOWNLOAD_NETWORK_ERROR},
    {103, U_CELL_FOTA_STATUS_DOWNLOAD_UNKNOWN_ERROR},
    {104, U_CELL_FOTA_STATUS_DOWNLOAD_BAD_URL},
    {105, U_CELL_FOTA_STATUS_DOWNLOAD_CONNECTIVITY_LOSS}
};

/** Array to convert the some of the +UUFWINSTALL URCs into
 * out install status enum.  Any not mentioned here are a one-to-one
 * mapping
 */
static uCellFotaStatusInstallConvert_t gInstallStatus[] = {
    {128, U_CELL_FOTA_STATUS_INSTALL_SUCCESS},
    {141, U_CELL_FOTA_STATUS_INSTALL_RAM_ERROR}
};

/* ----------------------------------------------------------------
 * STATIC FUNCTIONS
 * -------------------------------------------------------------- */

// Convert a download status fail case number into our enum.
static int32_t convertDownloadFailureStatus(int32_t atDownloadFailureStatus)
{
    int32_t downloadFailureStatus = -1;

    for (int32_t x = 0;
         (x < sizeof(gAtDownloadFailureStatus) / sizeof(gAtDownloadFailureStatus[0])) &&
         (downloadFailureStatus < 0); x++) {
        if (atDownloadFailureStatus == gAtDownloadFailureStatus[x].atInterfaceStatus) {
            downloadFailureStatus = (int32_t) gAtDownloadFailureStatus[x].status;
        }
    }

    return downloadFailureStatus;
}

// Convert a +UUFWINSTALL status value (i.e. ones from 128 upwards) into
// one of our install status enums
static int32_t convertUufwinstallStatus(int32_t uufwinstallStatus)
{
    int32_t installStatus = -1;

    for (int32_t x = 0;
         (x < sizeof(gInstallStatus) / sizeof(gInstallStatus[0])) &&
         (installStatus < 0); x++) {
        if (uufwinstallStatus == gInstallStatus[x].uufwInstallStatus) {
            installStatus = (int32_t) gInstallStatus[x].status;
        }
    }
    if (installStatus < 0) {
        // If there's no translation, just pass out what came in
        installStatus = uufwinstallStatus;
    }

    return installStatus;
}

// Callback via which the user's FOTA status callback is called.
// This must be called through the uAtClientCallback() mechanism in
// order to prevent customer code blocking the AT client.
static void fotaStatusCallback(uAtClientHandle_t atHandle, void *pParameter)
{
    uCellFotaStatusCallbackParameters_t *pCallback = (uCellFotaStatusCallbackParameters_t *) pParameter;

    (void) atHandle;

    if (pCallback != NULL) {
        if (pCallback->pCallback != NULL) {
            pCallback->pCallback(pCallback->cellHandle,
                                 &(pCallback->status),
                                 pCallback->pCallbackParam);
        }
        uPortFree(pCallback);
    }
}

// Call fotaStatusCallback() via the AT client callback queue.
static void queueFotaStatus(uCellPrivateInstance_t *pInstance,
                            uCellFotaStatus_t *pStatus)
{
    uCellPrivateFotaContext_t *pContext = (uCellPrivateFotaContext_t *) pInstance->pFotaContext;
    uCellFotaStatusCallbackParameters_t *pCallback;

    // Put all the data in a struct and pass a pointer to it to our
    // local callback via the AT client's callback mechanism to decouple
    // it from whatever might have called us.
    // Note: fotaStatusCallback will free the allocated memory.
    pCallback = (uCellFotaStatusCallbackParameters_t *) pUPortMalloc(sizeof(*pCallback));
    if (pCallback != NULL) {
        pCallback->cellHandle = pInstance->cellHandle;
        pCallback->status = *pStatus;
        pCallback->pCallback = pContext->pCallback;
        pCallback->pCallbackParam = pContext->pCallbackParameter;
        uAtClientCallback(pInstance->atHandle, fotaStatusCallback, pCallback);
    }
}

// The UFOSTAT URC callback.
static void UFOTASTAT_urc(uAtClientHandle_t atHandle,
                          void *pParameter)
{
    uCellFotaStatus_t status;
    int32_t event;
    int32_t param1;
    int32_t param2;
    bool urcIsGood = false;
    uCellPrivateInstance_t *pInstance = (uCellPrivateInstance_t *) pParameter;

    // Populate the status from the URC
    event = uAtClientReadInt(atHandle);
    // All events have two parameters
    param1 = uAtClientReadInt(atHandle);
    param2 = uAtClientReadInt(atHandle);
    if ((param1 >= 0) && (param2 >= 0)) {
        switch (event) {
            case 0: // download progress (percentage in param2)
                if ((param1 == 1) && (param2 >= 0)) {
                    status.type = U_CELL_FOTA_STATUS_TYPE_PERCENTAGE_DOWNLOAD;
                    status.value.percentage = param2;
                    urcIsGood = true;
                }
                break;
            case 1: // download start
                if ((param1 == 0) && (param2 == 0)) {
                    status.type = U_CELL_FOTA_STATUS_TYPE_DOWNLOAD;
                    // params tell us nothing of any use, just indicate
                    // a status of "start"
                    status.value.download = U_CELL_FOTA_STATUS_DOWNLOAD_START;
                    urcIsGood = true;
                }
                break;
            case 2: // download complete
                status.type = U_CELL_FOTA_STATUS_TYPE_DOWNLOAD;
                if ((param1 == 2) && (param2 == 100)) { // success
                    status.value.download = U_CELL_FOTA_STATUS_DOWNLOAD_SUCCESS;
                    urcIsGood = true;
                } else if (param1 == 3) { // NOT success!
                    status.value.download = convertDownloadFailureStatus(param2);
                    if (status.value.download >= 0) {
                        urcIsGood = true;
                    }
                }
                break;
            case 3: // FOTA update (or install) status
                if (param1 >= 0) { // Deliberately don't range check the top, better to let it through
                    status.type = U_CELL_FOTA_STATUS_TYPE_INSTALL;
                    // param1 is our enum exactly, param2 tells us nothing of interest
                    status.value.install = (uCellFotaStatusInstall_t) param1;
                    urcIsGood = true;
                }
                break;
        }

        if (urcIsGood) {
            queueFotaStatus(pInstance, &status);
        }
    }
}

// The URC callback for both the UFWPREVAL and UUFWINSTALL URCs.
static void UFWPREVAL_UUFWINSTALL_urc(uAtClientHandle_t atHandle,
                                      void *pParameter,
                                      uCellFotaStatusType_t type)
{
    uCellFotaStatus_t status;
    int32_t percentageOrStatusCode;
    uCellPrivateInstance_t *pInstance = (uCellPrivateInstance_t *) pParameter;

    percentageOrStatusCode = uAtClientReadInt(atHandle);
    if (percentageOrStatusCode >= 0) {
        if (percentageOrStatusCode < U_CELL_FOTA_STATUS_INSTALL_MIN_NUM_UUFWINSTALL) {
            status.type = type;
            status.value.percentage = percentageOrStatusCode;
        } else {
            // The +UUFWINSTALL URC sneaks error code in at the end
            // Some of these have the same meaning as those emitted
            // by the +UFOSTAT URC so some conversion is required
            percentageOrStatusCode = convertUufwinstallStatus(percentageOrStatusCode);
            status.type = U_CELL_FOTA_STATUS_TYPE_INSTALL;
            status.value.install = (uCellFotaStatusInstall_t) percentageOrStatusCode;
        }
        queueFotaStatus(pInstance, &status);
    }
}

// The UUFWPREVAL URC callback.
static void UFWPREVAL_urc(uAtClientHandle_t atHandle,
                          void *pParameter)
{
    UFWPREVAL_UUFWINSTALL_urc(atHandle, pParameter,
                              U_CELL_FOTA_STATUS_TYPE_PERCENTAGE_CHECK);
}

// The UUFWINSTALL URC callback.
static void UUFWINSTALL_urc(uAtClientHandle_t atHandle,
                            void *pParameter)
{
    UFWPREVAL_UUFWINSTALL_urc(atHandle, pParameter,
                              U_CELL_FOTA_STATUS_TYPE_PERCENTAGE_INSTALL);
}

/* ----------------------------------------------------------------
 * PUBLIC FUNCTIONS
 * -------------------------------------------------------------- */

// Enable or disable the FOTA status callback.
int32_t uCellFotaSetStatusCallback(uDeviceHandle_t cellHandle,
                                   int32_t modulePortNumber,
                                   uCellFotaStatusCallback_t *pCallback,
                                   void *pCallbackParameter)
{
    int32_t errorCode = (int32_t) U_ERROR_COMMON_NOT_INITIALISED;
    uCellPrivateInstance_t *pInstance;
    uAtClientHandle_t atHandle;
    uCellPrivateFotaContext_t *pContext;
    int32_t state = 0;

    if (gUCellPrivateMutex != NULL) {

        U_PORT_MUTEX_LOCK(gUCellPrivateMutex);

        pInstance = pUCellPrivateGetInstance(cellHandle);
        errorCode = (int32_t) U_ERROR_COMMON_INVALID_PARAMETER;
        if (pInstance != NULL) {
            errorCode = (int32_t) U_ERROR_COMMON_NOT_SUPPORTED;
            if (U_CELL_PRIVATE_HAS(pInstance->pModule,
                                   U_CELL_PRIVATE_FEATURE_FOTA)) {
                errorCode = (int32_t) U_ERROR_COMMON_NO_MEMORY;
                pContext = (uCellPrivateFotaContext_t *) pInstance->pFotaContext;
                if (pContext == NULL) {
                    // Note: we don't deallocate this until
                    // cellular is closed down in order to
                    // ensure thread-safety of the callback
                    pContext = (uCellPrivateFotaContext_t *) pUPortMalloc(sizeof(uCellPrivateFotaContext_t));
                }
                if (pContext != NULL) {
                    pInstance->pFotaContext = pContext;
                    errorCode = (int32_t) U_ERROR_COMMON_SUCCESS;
                    atHandle = pInstance->atHandle;
                    // Remove any existing URC handlers
                    uAtClientRemoveUrcHandler(atHandle, "+UFOTASTAT:");
                    uAtClientRemoveUrcHandler(atHandle, "+UFWPREVAL:");
                    uAtClientRemoveUrcHandler(atHandle, "+UUFWINSTALL:");
                    // Remember the new one
                    pContext->pCallback = pCallback;
                    pContext->pCallbackParameter = pCallbackParameter;
                    if (pCallback != NULL) {
                        state = 1;
                        errorCode = uAtClientSetUrcHandler(atHandle, "+UFOTASTAT:",
                                                           UFOTASTAT_urc, pInstance);
                    }
                    if (errorCode == 0) {
                        uAtClientLock(atHandle);
                        uAtClientCommandStart(atHandle, "AT+UFOTASTAT=");
                        uAtClientWriteInt(atHandle, state);
                        uAtClientCommandStopReadResponse(atHandle);
                        errorCode = uAtClientUnlock(atHandle);
                        if (errorCode == 0) {
                            if ((state == 1) &&
                                (uAtClientSetUrcHandler(atHandle, "+UFWPREVAL:",
                                                        UFWPREVAL_urc, pInstance) == 0) &&
                                (uAtClientSetUrcHandler(atHandle, "+UUFWINSTALL:",
                                                        UUFWINSTALL_urc, pInstance) == 0)) {
                                // Not all modules support the AT+UFWINSTALL
                                // command which is required to get the validation
                                // and installation progress (and it can only be
                                // switched on, not off); don't fail on this
                                uAtClientLock(atHandle);
                                uAtClientCommandStart(atHandle, "AT+UFWINSTALL=");
                                // Specify the port number if given
                                if (modulePortNumber >= 0) {
                                    uAtClientWriteInt(atHandle, modulePortNumber);
                                } else {
                                    uAtClientWriteString(atHandle, "", false);
                                }
                                // Skip the second and third parameters
                                uAtClientWriteString(atHandle, "", false);
                                uAtClientWriteString(atHandle, "", false);
                                uAtClientWriteInt(atHandle, state);
                                uAtClientCommandStopReadResponse(atHandle);
                                if (uAtClientUnlock(atHandle) != 0) {
                                    // Clean up on error
                                    uAtClientRemoveUrcHandler(atHandle, "+UUFWINSTALL:");
                                    uAtClientRemoveUrcHandler(atHandle, "+UFWPREVAL:");
                                }
                            }
                        } else {
                            // Clean up on error
                            pContext->pCallback = NULL;
                            pContext->pCallbackParameter = NULL;
                            uAtClientRemoveUrcHandler(atHandle, "+UFOTASTAT:");
                        }
                    }
                }
            }
        }

        U_PORT_MUTEX_UNLOCK(gUCellPrivateMutex);
    }

    return errorCode;
}

// End of file
