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

#ifndef U_CELL_FOTA_H_
#define U_CELL_FOTA_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _cell
 *  @{
 */

/** @file
 * @brief This header file defines the u-blox API for controlling/
 * monitoring FOTA (Firmware Over The Air) of a cellular module.
 * These functions are thread-safe.
 *
 * Note:  this is currently a minimal, monitor-only API; it may be
 * expanded in future.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The possible FOTA status types.
 */
typedef enum {
    U_CELL_FOTA_STATUS_TYPE_DOWNLOAD,
    U_CELL_FOTA_STATUS_TYPE_INSTALL,
    U_CELL_FOTA_STATUS_TYPE_PERCENTAGE_DOWNLOAD,
    U_CELL_FOTA_STATUS_TYPE_PERCENTAGE_CHECK, /**< not reported by all module types. */
    U_CELL_FOTA_STATUS_TYPE_PERCENTAGE_INSTALL, /**< not reported by all module types. */
    U_CELL_FOTA_STATUS_TYPE_MAX_NUM
} uCellFotaStatusType_t;

/** The possible FOTA download states.
 */
typedef enum {
    U_CELL_FOTA_STATUS_DOWNLOAD_START,
    U_CELL_FOTA_STATUS_DOWNLOAD_SUCCESS,
    U_CELL_FOTA_STATUS_DOWNLOAD_USER_CANCEL,
    U_CELL_FOTA_STATUS_DOWNLOAD_MEMORY_ERROR,
    U_CELL_FOTA_STATUS_DOWNLOAD_NETWORK_ERROR,
    U_CELL_FOTA_STATUS_DOWNLOAD_UNKNOWN_ERROR,
    U_CELL_FOTA_STATUS_DOWNLOAD_BAD_URL,
    U_CELL_FOTA_STATUS_DOWNLOAD_CONNECTIVITY_LOSS,
    U_CELL_FOTA_STATUS_DOWNLOAD_MAX_NUM
} uCellFotaStatusDownload_t;

/** The possible FOTA install states; values map to those
 * of the +UFOTASTAT and +UUFWINSTALL URCs.
 */
typedef enum {
    U_CELL_FOTA_STATUS_INSTALL_INITIAL = 0,
    U_CELL_FOTA_STATUS_INSTALL_SUCCESS = 1,
    U_CELL_FOTA_STATUS_INSTALL_MEMORY_ERROR = 2,
    U_CELL_FOTA_STATUS_INSTALL_RAM_ERROR = 3,
    U_CELL_FOTA_STATUS_INSTALL_CONNECTION_LOST = 4,
    U_CELL_FOTA_STATUS_INSTALL_CHECKSUM_ERROR = 5,
    U_CELL_FOTA_STATUS_INSTALL_UNSUPPORTED_PACKAGE = 6,
    U_CELL_FOTA_STATUS_INSTALL_URI_ERROR = 7,
    U_CELL_FOTA_STATUS_INSTALL_FIRMWARE_UPDATE_FAIL = 8,
    U_CELL_FOTA_STATUS_INSTALL_UNSUPPORTED_PROTOCOL = 9,
    U_CELL_FOTA_STATUS_INSTALL_USER_ABORT = 100, /**< SARA-R4 only. */
    // Error codes that are emitted by the +UUFWINSTALL URC begin here
    U_CELL_FOTA_STATUS_INSTALL_MIN_NUM_UUFWINSTALL = 128,
    // 128 is a repeat of the success case for the +UUFWINSTALL
    // URC and will be translated to U_CELL_FOTA_STATUS_INSTALL_SUCCESS
    U_CELL_FOTA_STATUS_INSTALL_GENERIC_FAIL = 129,
    U_CELL_FOTA_STATUS_INSTALL_FLASH_ACCESS_FAIL = 130,
    U_CELL_FOTA_STATUS_INSTALL_DELTA_FILE_ACCESS_FAIL = 131, /**< SARA-R5 only. */
    U_CELL_FOTA_STATUS_INSTALL_RAM_ALLOCATION_ERROR = 131, /**< SARA-R4/LARA-R6 only. */
    U_CELL_FOTA_STATUS_INSTALL_RETRIEVE_PARTITION_TABLE_ERROR = 132, /**< SARA-R4/LARA-R6 only. */
    U_CELL_FOTA_STATUS_INSTALL_BAD_BLOCK = 134, /**< SARA-R4/LARA-R6 only. */
    U_CELL_FOTA_STATUS_INSTALL_GENERIC_DECOMPOSITION_ENGINE_ERROR = 140,
    // 141 is a repeat of RAM error and will be translated to
    // U_CELL_FOTA_STATUS_INSTALL_RAM_ERROR
    U_CELL_FOTA_STATUS_INSTALL_FILE_NOT_FOUND = 144,
    U_CELL_FOTA_STATUS_INSTALL_FILE_SYSTEM_ACCESS_ERROR = 145, /**< SARA-R4/LARA-R6 only. */
    U_CELL_FOTA_STATUS_INSTALL_DELTA_FILE_CORRUPTED = 148,
    U_CELL_FOTA_STATUS_INSTALL_DELTA_FILE_FORMAT_NOT_RECOGNISED = 158,
    U_CELL_FOTA_STATUS_INSTALL_FLASH_WRITE_FAIL = 160,
    U_CELL_FOTA_STATUS_INSTALL_DELTA_FILE_FLASH_FIRMWARE_MISMATCH = 168,
    U_CELL_FOTA_STATUS_INSTALL_SIGNATURE_MISMATCH = 173,
    U_CELL_FOTA_STATUS_INSTALL_DELTA_FILE_VERSION_NOT_SUPPORTED = 174,
    U_CELL_FOTA_STATUS_INSTALL_RAM_CORRUPTION = 178,
    U_CELL_FOTA_STATUS_INSTALL_DELTA_FILE_SIZE_MISMATCH = 180,
    U_CELL_FOTA_STATUS_INSTALL_UPDATED_FLASH_CORRUPTION = 195,
    U_CELL_FOTA_STATUS_INSTALL_GENERIC_FINALIZATION_ERROR = 224,
    U_CELL_FOTA_STATUS_INSTALL_PUBLIC_KEY_ERROR = 227,
    U_CELL_FOTA_STATUS_INSTALL_FILE_OPERATION_FLASH_FAIL = 230, /**< SARA-R4/LARA-R6 only. */
    U_CELL_FOTA_STATUS_INSTALL_DELTA_FILE_PREVALIDATION_FAIL = 247
} uCellFotaStatusInstall_t;

/** A structure defining the overall FOTA status.
*/
typedef struct {
    uCellFotaStatusType_t type; /**< the type of status contained here. */
    union {
        uCellFotaStatusDownload_t download; /**< populated if type is #U_CELL_FOTA_STATUS_TYPE_DOWNLOAD. */
        uCellFotaStatusInstall_t install; /**< populated if type is #U_CELL_FOTA_STATUS_TYPE_INSTALL. */
        size_t percentage; /**< populated if type is #U_CELL_FOTA_STATUS_TYPE_PERCENTAGE_DOWNLOAD,
                                #U_CELL_FOTA_STATUS_TYPE_PERCENTAGE_CHECK or
                                #U_CELL_FOTA_STATUS_TYPE_PERCENTAGE_INSTALL. */
    } value;
} uCellFotaStatus_t;

/** Function signature of the FOTA status callback.
 */
typedef void (uCellFotaStatusCallback_t) (uDeviceHandle_t cellHandle,
                                          uCellFotaStatus_t *pStatus,
                                          void *pParameter);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Enable or disable a callback that will be provided with the
 * status of FOTA when it changes.
 *
 * @param cellHandle                  the handle of the cellular
 *                                    instance.
 * @param modulePortNumber            when a FW update is actually
 *                                    being installed the module needs
 *                                    to know which of its serial
 *                                    ports to send the progress
 *                                    updates over; the value to use
 *                                    is module specific (1 is always
 *                                    the UART port though) but to
 *                                    use the serial port on which you
 *                                    are currently communicating,
 *                                    specify -1.
 * @param[in] pCallback               pointer to the function to
 *                                    be given any FOTA status changes.
 *                                    Use NULL to deactivate a
 *                                    previously active FOTA status
 *                                    callback.
 * @param[in] pCallbackParameter      a pointer to be passed to
 *                                    the call-back as its last
 *                                    parameter; may be NULL.
 * @return                            zero on success or negative
 *                                    error code on failure.
 */
int32_t uCellFotaSetStatusCallback(uDeviceHandle_t cellHandle,
                                   int32_t modulePortNumber,
                                   uCellFotaStatusCallback_t *pCallback,
                                   void *pCallbackParameter);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // U_CELL_FOTA_H_