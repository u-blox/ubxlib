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

#ifndef _U_CELL_TIME_PRIVATE_H_
#define _U_CELL_TIME_PRIVATE_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** @file
 * @brief This header file defines a few types that are private
 * to CellTIme (so would normally be in u_cell_time.c) but also need to
 * be available to u_cell_private.c.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The CellTime context.
 */
typedef struct {
    void (*pCallbackEvent) (uDeviceHandle_t, uCellTimeEvent_t *, void *);
    void *pCallbackEventParam;
    void (*pCallbackTime) (uDeviceHandle_t, uCellTime_t *, void *);
    void *pCallbackTimeParam;
} uCellTimePrivateContext_t;

/** The CellTime cell synchronisation context.
 */
typedef struct {
    volatile int32_t errorCode;
    int32_t timingAdvance;
    int32_t cellIdPhysical;
} uCellTimeCellSyncPrivateContext_t;

#ifdef __cplusplus
}
#endif

#endif // _U_CELL_TIME_PRIVATE_H_

// End of file
