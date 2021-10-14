/*
 * Copyright 2020 u-blox
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

#ifndef _U_CELL_PWR_H_
#define _U_CELL_PWR_H_

/* No #includes allowed here */

/** @file
 * @brief This header file defines the APIs that initialse and
 * control power to a cellular module. These functions are
 * are thread-safe with the proviso that there is only one
 * cellular module underlying them.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** There can be an inverter in-line between the MCU pin
 * that is connected to the cellular module's RESET_N pin;
 * this allows the sense to be switched easily.
 */
#ifndef U_CELL_RESET_PIN_INVERTED
# define U_CELL_RESET_PIN_TOGGLE_TO_STATE 0
#else
# define U_CELL_RESET_PIN_TOGGLE_TO_STATE 1
#endif

/** The drive mode for the cellular module reset pin.
 */
#ifndef U_CELL_RESET_PIN_DRIVE_MODE
# if U_CELL_RESET_PIN_TOGGLE_TO_STATE == 0
/* Open drain so that we can pull RESET_N low and then
 * let it float afterwards since it is pulled-up by the
 * cellular module.
 */
#  define U_CELL_RESET_PIN_DRIVE_MODE U_PORT_GPIO_DRIVE_MODE_OPEN_DRAIN
# else
/* Normal mode since we're only driving the inverter that
 * must have been inserted between the MCU pin and the
 * cellular module RESET_N pin.
 */
#  define U_CELL_RESET_PIN_DRIVE_MODE U_PORT_GPIO_DRIVE_MODE_NORMAL
# endif
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Determine if the cellular module has power.  This is done
 * by checking the level on the Enable Power pin controlling power
 * to the module.  If there is no such pin, or if this cellular
 * driver has not been initialised so that it knows about the
 * pin, then this will return true.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            true if power is enabled to the module else
 *                    false.
 */
bool uCellPwrIsPowered(int32_t cellHandle);

/** Determine if the module is responsive.  It may happen that power
 * saving mode or some such gets out of sync, in which case this
 * can be called to see if the cellular module is responsive to
 * AT commands.  Note that, for the case where a module may power up
 * on its own, e.g. if no pin is connected to PWR_ON, it is not
 * sufficient to simply check for uCellPwrIsAlive() and continue:
 * uCellPwrOn() *must* always be called as it configures the cellular
 * module for correct operation with this driver (which
 * uCellPwrIsAlive() does not).
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            true if the module is responsive, else false.
 */
bool uCellPwrIsAlive(int32_t cellHandle);

/** Power the cellular module on.  If this function returns
 * success then the cellular module is ready to receive configuration
 * commands and register with the cellular network.  The caller
 * must have initialised this cellular instance by calling
 * uCellInit() and uCellAdd() before calling this function.
 * If both the pinPwrOn and pinEnablePower parameters to
 * uCellAdd() were -1, i.e. the PWR_ON pin of the module is not
 * being controlled and there is no need to enable the power supply
 * to the module, then this function will check that the module
 * is responsive and then configure it for correct operation
 * with this driver.
 *
 * @param cellHandle         the handle of the cellular instance.
 * @param pPin               pointer to a string giving the PIN of
 *                           the SIM. It is module dependent as to
 *                           whether this can be non-NULL; if it is
 *                           non-NULL and the module does not support
 *                           PIN entry (e.g. because it must always
 *                           be able to power-save and returning from
 *                           power saving mode requires the SIM to
 *                           power up without asking for a PIN)
 *                           then an error code will be returned.
 * @param pKeepGoingCallback power on usually takes between 5 and
 *                           15 seconds but it is possible for it
 *                           to take longer.  If this callback
 *                           function is non-NULL then it will
 *                           be called during the power-on
 *                           process and may be used to feed a
 *                           watchdog timer.  The callback
 *                           function should return true to
 *                           allow the power-on process to
 *                           be completed normally.  If the
 *                           callback function returns false
 *                           then the power-on process will
 *                           be abandoned.  Even when
 *                           this callback returns false it
 *                           this function may still take some
 *                           10's of seconds to return in order
 *                           to ensure that the module is in a
 *                           cleanly powered (or not) state.
 *                           If this function is forced to return
 *                           It is advisable to call
 *                           uCellPwrIsAlive() to confirm
 *                           the final state of the module. The
 *                           single int32_t parameter is the
 *                           cell handle.
 * @return                   zero on success or negative error
 *                           code on failure.
 */
int32_t uCellPwrOn(int32_t cellHandle, const char *pPin,
                   bool (*pKeepGoingCallback) (int32_t));

/** Power the cellular module off.
 *
 * @param cellHandle         the handle of the cellular instance.
 * @param pKeepGoingCallback it is possible for power off to
 *                           take some time.  If this callback
 *                           function is non-NULL then it will
 *                           be called during the power-off
 *                           process and may be used to feed a
 *                           watchdog timer.  The callback
 *                           function should return true to
 *                           allow the power-off process to
 *                           be completed normally.  If the
 *                           callback function returns false
 *                           then the power-off process will
 *                           be forced to completion immediately
 *                           and this function will return.
 *                           It is advisable for the callback
 *                           function to always return true,
 *                           allowing the cellular module to
 *                           power off cleanly. The single int32_t
 *                           parameter is the cell handle.
 * @return                   zero on success or negative error
 *                           code on failure.
 */
int32_t uCellPwrOff(int32_t cellHandle,
                    bool (*pKeepGoingCallback) (int32_t));

/** Remove power to the cellular module using HW lines.
 * If both the pinPwrOn and pinEnablePower parameters to
 * uCellAdd() were -1, i.e. the PWR_ON pin of the module is not
 * being controlled and there is no way to disable the power
 * supply to the module, then this function will return an error.
 *
 * @param cellHandle         the handle of the cellular instance.
 * @param trulyHard          if this is set to true and a
 *                           non-negative value for pinEnablePower
 *                           was supplied to uCellInit()
 *                           then just pull the power to the
 *                           cellular module.  ONLY USE IN
 *                           EMERGENCIES, IF THE CELLULAR MODULE
 *                           HAS BECOME COMPLETELY UNRESPONSIVE.
 *                           If a negative value for pinEnablePower
 *                           was supplied this value is treated
 *                           as false.
 * @param pKeepGoingCallback even with HW lines powering the
 *                           cellular module off it is possible
 *                           for power off to take some time.
 *                           If this callback function is
 *                           non-NULL then it will be called
 *                           during the power-off process and
 *                           may be used to feed a watchdog
 *                           timer.  The callback function
 *                           should return true to allow the
 *                           power-off process to be completed
 *                           normally.  If the callback function
 *                           returns false then the power-off process
 *                           will be forced to completion immediately
 *                           and this function will return.
 *                           It is advisable for the callback
 *                           function to always return true,
 *                           allowing the cellular module to
 *                           power off cleanly. The single int32_t
 *                           parameter is the cell handle.
 *                           Ignored if trulyHard is true.
 * @return                   zero on success or negative error
 *                           code on failure.
 */
int32_t uCellPwrOffHard(int32_t cellHandle, bool trulyHard,
                        bool (*pKeepGoingCallback) (int32_t));


/** If a reboot of the cellular instance is required, e.g.
 * due to changes that have been made to the configuration,
 * this will return true. uCellPwrReboot() should be called
 * to effect the reboot.
 *
 * @param cellHandle  the handle of the cellular instance.
 */
bool uCellPwrRebootIsRequired(int32_t cellHandle);

/** Re-boot the cellular module.  The module will be reset after
 * a proper detach from the network and any NV parameters will
 * be saved.  If this function returns successfully then the
 * module is ready for immediate use, no call to uCellPwrOn()
 * is required (since the SIM is not reset by a reboot).
 * TODO: is the bit about the SIM above true in all cases?
 *
 * @param cellHandle         the handle of the cellular instance.
 * @param pKeepGoingCallback rebooting usually takes between 5 and
 *                           15 seconds but it is possible for it
 *                           to take longer.  If this callback
 *                           function is non-NULL then it will
 *                           be called during the re-boot
 *                           process and may be used to feed a
 *                           watchdog timer.  The callback
 *                           function should return true to
 *                           allow the re-boot process to
 *                           be completed normally.  If the
 *                           callback function returns false
 *                           then the re-boot process will
 *                           be abandoned.  Even when
 *                           this callback returns false this
 *                           function may still take some
 *                           10's of seconds to return in order
 *                           to ensure that the module is in a
 *                           cleanly powered (or not) state.
 *                           If this function is forced to return
 *                           It is advisable to call
 *                           uCellPwrIsAlive() to confirm
 *                           the final state of the module. The
 *                           single int32_t parameter is the
 *                           cell handle.
 * @return                   zero on success or negative error
 *                           code on failure.
 */
int32_t uCellPwrReboot(int32_t cellHandle,
                       bool (*pKeepGoingCallback) (int32_t));

/** Reset the cellular module using the given MCU pin, which should
 * be connected to the reset pin of the cellular module, e.g.
 * U_CFG_APP_PIN_CELL_RESET could be used.  Note that NO organised
 * network detach is carried out; this is a hard reset and hence
 * should be used only in emergencies if, for some reason,
 * AT communication with the cellular module has totally failed.
 * Note also that for some modules this function may not return for
 * some considerable time (e.g. the reset line has to be held for
 * 16 seconds to reset a SARA-R4 series module).
 *
 * @param cellHandle the handle of the cellular instance.
 * @param pinReset   the pin of the MCU that is connected to the
 *                   reset pin of the cellular module.
 * @return           zero on success or negative error
 *                   code on failure.
 */
int32_t uCellPwrResetHard(int32_t cellHandle, int32_t pinReset);

#ifdef __cplusplus
}
#endif

#endif // _U_CELL_PWR_H_

// End of file
