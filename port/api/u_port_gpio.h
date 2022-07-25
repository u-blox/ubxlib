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

#ifndef _U_PORT_GPIO_H_
#define _U_PORT_GPIO_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup __port
 *  @{
 */

/** @file
 * @brief Porting layer for GPIO access functions.  These functions
 * are thread-safe.
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

/** The possible GPIO directions.
 */
typedef enum {
    U_PORT_GPIO_DIRECTION_NONE,
    U_PORT_GPIO_DIRECTION_INPUT,
    U_PORT_GPIO_DIRECTION_OUTPUT,
    U_PORT_GPIO_DIRECTION_INPUT_OUTPUT,
    MAX_NUM_U_PORT_GPIO_DIRECTIONS
} uPortGpioDirection_t;

/** The possible GPIO pull modes.
 */
typedef enum {
    U_PORT_GPIO_PULL_MODE_NONE,
    U_PORT_GPIO_PULL_MODE_PULL_UP,
    U_PORT_GPIO_PULL_MODE_PULL_DOWN,
    MAX_NUM_U_PORT_GPIO_PULL_MODES
} uPortGpioPullMode_t;

/** The possible GPIO drive modes.
 */
typedef enum {
    U_PORT_GPIO_DRIVE_MODE_NORMAL,
    U_PORT_GPIO_DRIVE_MODE_OPEN_DRAIN,
    MAX_NUM_U_PORT_GPIO_DRIVE_MODES
} uPortGpioDriveMode_t;

/** The possible GPIO drive capabilities.
 * A number from 0 to 3 where 0 is weakest.
 */
typedef enum {
    U_PORT_GPIO_DRIVE_CAPABILITY_WEAKEST = 0,
    U_PORT_GPIO_DRIVE_CAPABILITY_WEAK = 1,
    U_PORT_GPIO_DRIVE_CAPABILITY_STRONG = 2,
    U_PORT_GPIO_DRIVE_CAPABILITY_STRONGEST = 3,
    MAX_NUM_U_PORT_GPIO_DRIVE_CAPABILITIES
} uPortGpioDriveCapability_t;

/** GPIO configuration structure.
 * If you update this, don't forget to update
 * #U_PORT_GPIO_CONFIG_DEFAULT and
 * #U_PORT_GPIO_SET_DEFAULT also.
 */
typedef struct {
    int32_t pin;  /**< a positive integer; note that the pin number is
                       that of the MCU: if you are using an MCU inside
                       a u-blox module the IO pin numbering for the
                       module is likely different to that from the MCU,
                       check the data sheet for the module to determine
                       the mapping. */
    uPortGpioDirection_t direction;
    uPortGpioPullMode_t pullMode;
    uPortGpioDriveMode_t driveMode;
    uPortGpioDriveCapability_t driveCapability;
} uPortGpioConfig_t;

/** Default values for the above.
 */
#define U_PORT_GPIO_CONFIG_DEFAULT {-1,                                 \
                                   U_PORT_GPIO_DIRECTION_NONE,          \
                                   U_PORT_GPIO_PULL_MODE_NONE,          \
                                   U_PORT_GPIO_DRIVE_MODE_NORMAL,       \
                                   U_PORT_GPIO_DRIVE_CAPABILITY_STRONG}

/** Compilers won't generally allow myConfig = #U_PORT_GPIO_CONFIG_DEFAULT;
 * to be done anywhere other than where myConfig is declared.  This macro
 * provides a method to do that.
 */
#define U_PORT_GPIO_SET_DEFAULT(pConfig) (pConfig)->pin = -1;                                              \
                                         (pConfig)->direction = U_PORT_GPIO_DIRECTION_NONE;                \
                                         (pConfig)->pullMode = U_PORT_GPIO_PULL_MODE_NONE;                 \
                                         (pConfig)->driveMode = U_PORT_GPIO_DRIVE_MODE_NORMAL;             \
                                         (pConfig)->driveCapability = U_PORT_GPIO_DRIVE_CAPABILITY_STRONG

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Configure a GPIO pin.
 *
 * @param[in] pConfig a pointer to the configuration to set, cannot be
 *                    NULL.
 * @return            zero on success else negative error code.
 */
int32_t uPortGpioConfig(uPortGpioConfig_t *pConfig);

/** Set the state of a GPIO pin.  Note that the pin number is
 * that of the MCU: if you are using an MCU inside a u-blox
 * module the IO pin numbering for the module is likely different
 * to that from the MCU: check the data sheet for the module to
 * determine the mapping.
 *
 * @param pin   the pin to set, a positive integer.
 * @param level the level to set, 0 for low or non-zero for high.
 * @return      zero on success else negative error code.
 */
int32_t uPortGpioSet(int32_t pin, int32_t level);

/** Get the state of a GPIO pin.  Note that the pin number is
 * that of the MCU: if you are using an MCU inside a u-blox
 * module the IO pin numbering for the module is likely different
 * to that from the MCU: check the data sheet for the module to
 * determine the mapping.
 *
 * @param pin   the pin to get the state of, a positive integer.
 * @return      on success the level (0 or 1) else negative error
 *              code.
 */
int32_t uPortGpioGet(int32_t pin);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_PORT_GPIO_H_

// End of file
