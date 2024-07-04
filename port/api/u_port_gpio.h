/*
 * Copyright 2019-2024 u-blox
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
 * Not all platforms support setting the
 * drive strength.
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
    int32_t index; /**< currently only relevant for Linux, ignored
                        otherwise, set to -1 to indicate the default;
                        this is used to inform this driver which of
                        a set of GPIO chips the given pin is on.
                        Note that the pin number must still be unique
                        across all GPIO chips: for example if the
                        last pin on GPIO chip 0 were pin 15 then the
                        first pin on GPIO chip 1 would likely be pin 16,
                        it could not be pin 0 again. */
    void (*pInterrupt)(void); /**< if non-NULL and interrupts are supported
                                   by the platform then the pin will be
                                   configured as an interrupt; NULL is the
                                   default. If you have your own port you
                                   only need to implement interrupt functionality
                                   if you wish to use the "data ready" feature
                                   of the GNSS interface, enabling this MCU to
                                   sleep while waiting for a response from
                                   a GNSS device; GPIO interrupts are otherwise
                                   not used within ubxlib.  Note also that
                                   some platforms may require additional
                                   compile-time configuration for this to work,
                                   e.g. for STM32Cube it is necessary to make
                                   the correct HW interrupts available to
                                   this code, search for
                                   U_CFG_HW_EXTI_ to find out more; also,
                                   platforms may apply additional restrictions,
                                   e.g. an interrupt pin may not be able to
                                   be set as input/output (this is the case
                                   with ESP32), perhaps only certain pins
                                   can be set as interrupts, etc. */
    bool interruptActiveLow; /**< if true then the pin is assumed
                                  to be an active-low interrupt, else
                                  (the default) if is assumed to be an
                                  active-high interrupt; ignored if
                                  pInterrupt is NULL or interrupts are
                                  not supported by the platform. */
    bool interruptLevel;  /**< if true then the pin will be configured
                               as a level-triggered interrupt, else
                               it will be configured as an edge-triggered
                               interrupt (the default); ignored if
                               pInterrupt is NULL or interrupts are not
                               supported by the platform, not all platforms
                               support level-triggered interrupts (e.g.
                               STM32F4 does not). */
} uPortGpioConfig_t;

/** Default values for the above.
 */
#define U_PORT_GPIO_CONFIG_DEFAULT {-1,                                 \
                                   U_PORT_GPIO_DIRECTION_NONE,          \
                                   U_PORT_GPIO_PULL_MODE_NONE,          \
                                   U_PORT_GPIO_DRIVE_MODE_NORMAL,       \
                                   U_PORT_GPIO_DRIVE_CAPABILITY_STRONG, \
                                   -1,                                  \
                                   NULL, false, false}

/** Compilers won't generally allow myConfig = #U_PORT_GPIO_CONFIG_DEFAULT;
 * to be done anywhere other than where myConfig is declared.  This macro
 * provides a method to do that.
 */
#define U_PORT_GPIO_SET_DEFAULT(pConfig) (pConfig)->pin = -1;                                              \
                                         (pConfig)->direction = U_PORT_GPIO_DIRECTION_NONE;                \
                                         (pConfig)->pullMode = U_PORT_GPIO_PULL_MODE_NONE;                 \
                                         (pConfig)->driveMode = U_PORT_GPIO_DRIVE_MODE_NORMAL;             \
                                         (pConfig)->driveCapability = U_PORT_GPIO_DRIVE_CAPABILITY_STRONG; \
                                         (pConfig)->index = -1;                                            \
                                         (pConfig)->pInterrupt = NULL;                                     \
                                         (pConfig)->interruptActiveLow = false;                            \
                                         (pConfig)->interruptLevel = false;

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Configure a GPIO pin.
 *
 * @param[in] pConfig a pointer to the configuration to set, cannot be
 *                    NULL; it is good practice to initialise the
 *                    configuration to #U_PORT_GPIO_CONFIG_DEFAULT
 *                    and then modify any values that you want different.
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

/** Should return true if interrupts are supported; where not
 * supported a weak implementation will return false.
 *
 * @return true if interrupts are supported, else false.
 */
bool uPortGpioInterruptSupported();

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_PORT_GPIO_H_

// End of file
