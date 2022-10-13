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

#ifndef _U_CELL_HTTP_PRIVATE_H_
#define _U_CELL_HTTP_PRIVATE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines a few HTTP types that are private
 * to HTTP (so would normally be in u_cell_http.c) but also need to
 * be available to u_cell_private.c.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** Definition of an HTTP instance, designed to be used in a linked-list.
 */
typedef struct uCellHttpInstance_t {
    int32_t profileId;    /**< this will be the handle for the HTTP instance. */
    int32_t timeoutSeconds;
    uCellHttpCallback_t *pCallback;
    void *pCallbackParam;
    char fileNameResponse[U_CELL_FILE_NAME_MAX_LENGTH + 1];
    struct uCellHttpInstance_t *pNext;
} uCellHttpInstance_t;

/** HTTP context data, one for each cellular instance.
 */
typedef struct {
    int32_t eventQueueHandle;
    uPortMutexHandle_t linkedListMutex;
    uCellHttpInstance_t *pInstanceList;
} uCellHttpContext_t;

#ifdef __cplusplus
}
#endif

#endif // _U_CELL_HTTP_PRIVATE_H_

// End of file
