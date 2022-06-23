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

#ifndef U_CELL_GPIO_H_
#define U_CELL_GPIO_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _cell
 *  @{
 */

/** @file
 * @brief This header file defines the u-blox API for controlling
 * the GPIO lines of a cellular module that is attached to this MCU.
 * These functions are thread-safe.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** Macro helper: the APIs here use the GPIO ID, exactly as defined
 * in the u-blox AT command manual, which is NOT the number on the
 * end of the GPIO name, so "GPIO1" is NOT GPIO ID 1, it is
 * GPIO ID 16, and hence the APIs use the #uCellGpioName_t enum
 * below to make this clearer.  However,  if in your code you wish
 * to use the integer x on the end of "GPIOx" as your identifier
 * then you may use the macro below to map your integer to the
 * GPIO ID required in this API.
 */
#define U_CELL_GPIO_NUMBER_TO_GPIO_ID(num)  ((num) == 1 ? U_CELL_GPIO_1 : (num) == 2 ? U_CELL_GPIO_2 : \
                                             (num) == 3 ? U_CELL_GPIO_3 : (num) == 4 ? U_CELL_GPIO_4 : \
                                             (num) == 5 ? U_CELL_GPIO_5 : (num) == 6 ? U_CELL_GPIO_6 : U_CELL_GPIO_UNKNOWN)

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The GPIO names, which map to GPIO IDs.  Note that not all
 * modules support all GPIOs.
 */
typedef enum {
    U_CELL_GPIO_UNKNOWN = -1,
    U_CELL_GPIO_1 = 16,
    U_CELL_GPIO_2 = 23,
    U_CELL_GPIO_3 = 24,
    U_CELL_GPIO_4 = 25,
    U_CELL_GPIO_5 = 42,
    U_CELL_GPIO_6 = 19
} uCellGpioName_t;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Configure a GPIO of a cellular module.
 * VERY IMPORTANT: adopting the terminology of the u-blox AT commmand
 * manual, each cellular module pin may be referred to in three ways:
 *
 *  - pin number: the physical pin of the cellular module,
 *  - GPIO ID: the ID for that pin, which is usually THE SAME AS
 *    THE PIN NUMBER,
 *  - pin name: for instance "GPIO1" or "SDIO_CMD" etc.
 *
 * This API uses GPIO ID: do not confuse this with the number on
 * the end of the pin name, i.e. "GPIO1" is NOT GPIO ID 1, it is GPIO
 * ID 16!  Hence the #uCellGpioName_t enum is used to allow you to
 * pass in #U_CELL_GPIO_1, the value for which is 16. If you prefer
 * to use plain integers in your code you can just pass in the GPIO IDs
 * (i.e. 16 etc., the ones which usually map to the physical pin
 * number) and that will also work fine.
 *
 * @param cellHandle    the handle of the cellular instance.
 * @param gpioId        the GPIO ID to set.
 * @param isOutput      the direction, set to true for an output, false for
 *                      an input.
 * @param level         the initial level to set, only used if isOutput
 *                      is true; 0 for low or non-zero for high.
 * @return              zero on success else negative error code.
 */
int32_t uCellGpioConfig(uDeviceHandle_t cellHandle, uCellGpioName_t gpioId,
                        bool isOutput, int32_t level);

/** Set the state of a GPIO of a cellular module.
 * VERY IMPORTANT: adopting the terminology of the u-blox AT commmand
 * manual, each cellular module pin may be referred to in three ways:
 *
 *  - pin number: the physical pin of the cellular module,
 *  - GPIO ID: the ID for that pin, which is usually THE SAME AS
 *    THE PIN NUMBER,
 *  - pin name: for instance "GPIO1" or "SDIO_CMD" etc.
 *
 * This API uses GPIO ID: do not confuse this with the number on
 * the end of the pin name, i.e. "GPIO1" is NOT GPIO ID 1, it is GPIO
 * ID 16!  Hence the #uCellGpioName_t enum is used to allow you to
 * pass in #U_CELL_GPIO_1, the value for which is 16. If you prefer
 * to use plain integers in your code you can just pass in the GPIO IDs
 * (i.e. 16 etc., the ones which usually map to the physical pin
 * number) and that will also work fine.
 *
 * @param cellHandle    the handle of the cellular instance.
 * @param gpioId        the GPIO ID to set.
 * @param level         the level to set, 0 for low or non-zero for high.
 * @return              zero on success else negative error code.
 */
int32_t uCellGpioSet(uDeviceHandle_t cellHandle, uCellGpioName_t gpioId,
                     int32_t level);

/** Get the state of a GPIO of a cellular module.
 * VERY IMPORTANT: adopting the terminology of the u-blox AT commmand
 * manual, each cellular module pin may be referred to in three ways:
 *
 *  - pin number: the physical pin of the cellular module,
 *  - GPIO ID: the ID for that pin, which is usually THE SAME AS
 *    THE PIN NUMBER,
 *  - pin name: for instance "GPIO1" or "SDIO_CMD" etc.
 *
 * This API uses GPIO ID: do not confuse this with the number on
 * the end of the pin name, i.e. "GPIO1" is NOT GPIO ID 1, it is GPIO
 * ID 16!  Hence the #uCellGpioName_t enum is used to allow you to
 * pass in #U_CELL_GPIO_1, the value for which is 16. If you prefer
 * to use plain integers in your code you can just pass in the GPIO IDs
 * (i.e. 16 etc., the ones which usually map to the physical pin
 * number) and that will also work fine.
 *
 * @param cellHandle    the handle of the cellular instance.
 * @param gpioId        the GPIO ID to get the state of.
 * @return              on success the level 0 (low) or 1 (high)
 *                      else negative error code.
 */
int32_t uCellGpioGet(uDeviceHandle_t cellHandle, uCellGpioName_t gpioId);

/** Set the state of the CTS line: this may be used if the
 * serial handshaking lines are NOT being used (they were both
 * -1 in the #uNetworkCfgCell_t structure or the
 * call to uPortUartOpen(), or you may call
 * uCellInfoIsCtsFlowControlEnabled() to determine the truth).
 * Note that NOT all modules support this feature (e.g. SARA-R4
 * modules do not).
 *
 * @param cellHandle    the handle of the cellular instance.
 * @param level         the level to set, 0 for low or non-zero for high.
 * @return              zero on success else negative error code.
 */
int32_t uCellGpioSetCts(uDeviceHandle_t cellHandle, int32_t level);

/** Get the state of the CTS line: this may be used if the
 * serial handshaking lines are NOT being used (when they are both
 * -1 in the #uNetworkCfgCell_t structure or the
 * call to uPortUartOpen(), or you may call
 * uCellInfoIsCtsFlowControlEnabled() to determine the truth).
 * Note that NOT all modules support this feature (e.g. SARA-R4
 * modules do not).
 *
 * @param cellHandle    the handle of the cellular instance.
 * @return              on success the level 0 (low) or 1 (high)
 *                      else negative error code.
 */
int32_t uCellGpioGetCts(uDeviceHandle_t cellHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // U_CELL_GPIO_H_