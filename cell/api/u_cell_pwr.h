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

#ifndef _U_CELL_PWR_H_
#define _U_CELL_PWR_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

/** \addtogroup _cell
 *  @{
 */

/** @file
 * @brief This header file defines the APIs that initialse and
 * control power to a cellular module and enable it to save power
 * through sleep.  These functions are thread-safe.
 *
 * NOTES ON POWER SAVING
 * A u-blox cellular module has two sleep states and three ways to
 * get to them.  You can read more detail below but, in summary:
 *
 * - this code automatically configures the cellular module for
 *   "32 kHz sleep"; it can do this because this sleep mode has no
 *   adverse effect on the application, does not need to be configured
 *   by the application, etc.,
 * - a typical application may then configure E-DRX, with timings of the
 *   application's choosing, to save more power by allowing the module
 *   to switch its radio off for longer periods,
 * - a very sleepy application, one which perhaps wakes up just a few
 *   times a day, may instead configure 3GPP sleep to save the most
 *   power, provided that application is happy to lose all module state
 *   (sockets, MQTT broker connections, etc.) on entry to sleep.
 *
 * The sleep states are as follows:
 *
 * "UART sleep"/"32 kHz sleep": in this sleep state the speed of the
 * module's clocks are reduced to save a lot of power.  Because of
 * these reduced clock rates the module is not able to drive the
 * UART HW, hence this is often termed "UART sleep".  However, all
 * of the module's RAM is still on, state is fully retained, the module
 * is still actually running, is still connected to the network, and
 * it can be woken-up quickly by toggling lines of the UART AT interface.
 *
 * "deep sleep": in this sleep state the module is basically off,
 * almost all state is lost, what is retained is only a basic notion
 * of time and whether the module was attached to the cellular
 * network when deep sleep began.  The IP stack on the module, the
 * MQTT client on the module, etc, are all reset by deep sleep.
 *
 * The ways of entering these sleep states are as follows:
 *
 * "AT+UPSV": this command permits the module to enter "32 kHz sleep"
 * after a given amount of inactivity.  This code enables AT+UPSV power
 * saving automatically with a timer of 6 seconds and wakes the module
 * up again as required by the application.  You need do nothing unless
 * you have a LARA-R6 module, which requires the DTR pin to be employed,
 * see uCellPwrSetDtrPowerSavingPin().
 *
 * "E-DRX": this is 3GPP-defined and forms an agreement with the network
 * that the module will be out of contact for short periods (think 10's
 * or 100's, at most 1000's of seconds) so that the module can save power.
 * The functions with "EDrx" in the name below allow you to initiate and
 * manage E-DRX.  This is something you, the application writer, must
 * do, since the timings, the required wakefulness, is something only
 * the application can know.  During the "sleep" periods of E-DRX,
 * because this code always engages "AT+UPSV", the module is in 32 kHz
 * sleep but it can also power the cellular radio down and hence save
 * a lot more power.  And because this code only allows the module to
 * go into 32 kHz sleep during the E-DRX sleep periods the application
 * never has to worry about state being lost.
 *
 * "3GPP power saving made (PSM)": also a 3GPP-defined mechanism, this
 * forms an agreement with the network that the module will be out of
 * contact for long periods (think hours or days).  The functions below
 * with "3gppPowerSaving" in the name allow you to initiate and manage
 * 3GPP power saving.  During the sleep periods of 3GPP power saving mode
 * the module enters deep sleep, all state aside from the knowledge of
 * its cellular connection with the network is lost; module sockets/MQTT,
 * etc. are reset.  It is like the module is actually switched off except
 * that the network _knows_ it is off and maintains that knowledge so
 * that when the module leaves deep sleep it doesn't necessarily have
 * to contact the network to tell it, the two are behaving according to
 * their 3GPP power saving agreement.  Since the module is almost entirely
 * off during 3GPP sleep things such as waiting for an answer from a cloud
 * service, waiting for an attached GNSS module to do something, all of
 * these long-term things, will be curtailed if the deep sleep were to
 * be entered; it is up to the application writer to ensure that 3GPP
 * power saving is configured appropriately, considering what the cellular
 * module has been asked to do.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------
 * COMPILE-TIME MACROS
 * -------------------------------------------------------------- */

/** There can be an inverter in-line between the MCU pin
 * that is connected to the cellular module's RESET_N pin;
 * this allows the sense to be switched at compile time.
 * However, the method of ORing the pin with
 * #U_CELL_PIN_INVERTED (see u_cell.h) is preferred; this
 * compile-time mechanism is retained for backwards-compatibility.
 * DON'T USE BOTH MECHANISMS or the sense of the pin will
 * be inverted twice.
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

/** There can be an inverter in-line between the MCU pin
 * that is connected to the cellular module's DTR pin and the
 * module's DTR pin itself; this allows the sense to be switched
 * at compile time. However, the method of ORing the pin
 * with #U_CELL_PIN_INVERTED (see u_cell.h) is preferred; this
 * compile-time mechanism is retained for backwards-compatibility.
 * DON'T USE BOTH MECHANISMS or the sense of the pin will
 * be inverted twice.
 * See uCellPwrSetDtrPowerSavingPin() for how the pin value is set.
 */
#ifndef U_CELL_DTR_PIN_INVERTED
# define U_CELL_DTR_PIN_ON_STATE 0
#else
# define U_CELL_DTR_PIN_ON_STATE 1
#endif

#ifndef U_CELL_PWR_UART_POWER_SAVING_DTR_READY_MS
/** When DTR power saving is in use (see uCellPwrSetDtrPowerSavingPin()),
 * this is how long to wait after DTR has been asserted before
 * the module is ready to receive UART data; value in milliseconds.
 */
# define U_CELL_PWR_UART_POWER_SAVING_DTR_READY_MS 20
#endif

#ifndef U_CELL_PWR_UART_POWER_SAVING_DTR_HYSTERESIS_MS
/** When DTR power saving is in use (see uCellPwrSetDtrPowerSavingPin()),
 * this is the minimum time that should pass betweeen toggling of the pin;
 * value in milliseconds.
 */
# define U_CELL_PWR_UART_POWER_SAVING_DTR_HYSTERESIS_MS 20
#endif

/* ----------------------------------------------------------------
 * TYPES
 * -------------------------------------------------------------- */

/** The possible 3GPP power saving states: not all modules that support
 * 3GPP power saving are able to signal all states.
 */
typedef enum {
    U_CELL_PWR_3GPP_POWER_SAVING_STATE_UNKNOWN = 0,
    U_CELL_PWR_3GPP_POWER_SAVING_STATE_NOT_SUPPORTED, /**< 3GPP power saving
                                                           is not supported
                                                           by the module. */
    U_CELL_PWR_3GPP_POWER_SAVING_STATE_AVAILABLE,    /**< 3GPP power saving is possible
                                                          but is either not switched on
                                                          or is not allowed by the
                                                          network. */
    U_CELL_PWR_3GPP_POWER_SAVING_STATE_AGREED_BY_NETWORK, /**< the 3GPP power
                                                               saving parameters
                                                               have been agreed
                                                               with the network
                                                               (use
                                                               uCellPwrGet3gppPowerSaving()
                                                               to read them)
                                                               but 3GPP power
                                                               saving is not
                                                               currently active. */
    U_CELL_PWR_3GPP_POWER_SAVING_STATE_BLOCKED_BY_NETWORK, /**< 3GPP power saving has been
                                                                agreed with the network but
                                                                is not currently allowed
                                                                by the network (so the last
                                                                registration indication
                                                                received from the module does
                                                                not include the 3GPP power
                                                                saving parameters even though
                                                                3GPP power saving was
                                                                previously agreed). */
    U_CELL_PWR_3GPP_POWER_SAVING_STATE_BLOCKED_BY_MODULE, /**< 3GPP power saving could be
                                                               active but one or more
                                                               applications (IP stack
                                                               or MQTT or HTTP or LWM2M or
                                                               GNSS) on the module is
                                                               blocking it. */
    U_CELL_PWR_3GPP_POWER_SAVING_STATE_ACTIVE,      /**< the cellular protocol stack on
                                                         the module has entered 3GPP
                                                         power saving. */
    U_CELL_PWR_3GPP_POWER_SAVING_STATE_ACTIVE_DEEP_SLEEP_ACTIVE, /**< the cellular
                                                                      protocol stack on
                                                                      the module has
                                                                      entered 3GPP power
                                                                      saving and the
                                                                      module HW has been
                                                                      able to take
                                                                      advantage of this
                                                                      and has entered
                                                                      deep sleep; this
                                                                      state can only be
                                                                      determined if a pin
                                                                      of this MCU is
                                                                      connected to the
                                                                      VInt pin of the
                                                                      module. */
    U_CELL_PWR_3GPP_POWER_SAVING_STATE_MAX_NUM
} uCellPwr3gppPowerSavingState_t;

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
bool uCellPwrIsPowered(uDeviceHandle_t cellHandle);

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
bool uCellPwrIsAlive(uDeviceHandle_t cellHandle);

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
 * @param cellHandle             the handle of the cellular instance.
 * @param[in] pSimPinCode        pointer to a string giving the PIN of
 *                               the SIM. It is module dependent as to
 *                               whether this can be non-NULL; if it is
 *                               non-NULL and the module does not support
 *                               PIN entry (e.g. because it must always
 *                               be able to power-save and returning from
 *                               power saving mode requires the SIM to
 *                               power up without asking for a PIN)
 *                               then an error code will be returned.
 * @param[in] pKeepGoingCallback power on usually takes between 5 and
 *                               15 seconds but it is possible for it
 *                               to take longer.  If this callback
 *                               function is non-NULL then it will
 *                               be called during the power-on
 *                               process and may be used to feed a
 *                               watchdog timer.  The callback
 *                               function should return true to
 *                               allow the power-on process to
 *                               be completed normally.  If the
 *                               callback function returns false
 *                               then the power-on process will
 *                               be abandoned.  Even when
 *                               this callback returns false
 *                               this function may still take some
 *                               10's of seconds to return in order
 *                               to ensure that the module is in a
 *                               cleanly powered (or not) state.
 *                               If this function is forced to return
 *                               it is advisable to call
 *                               uCellPwrIsAlive() to confirm
 *                               the final state of the module.
 * @return                       zero on success or negative error
 *                               code on failure.
 */
int32_t uCellPwrOn(uDeviceHandle_t cellHandle,
                   const char *pSimPinCode,
                   bool (*pKeepGoingCallback) (uDeviceHandle_t));

/** Power the cellular module off.
 *
 * @param cellHandle             the handle of the cellular instance.
 * @param[in] pKeepGoingCallback it is possible for power off to
 *                               take some time.  If this callback
 *                               function is non-NULL then it will
 *                               be called during the power-off
 *                               process and may be used to feed a
 *                               watchdog timer.  The callback
 *                               function should return true to
 *                               allow the power-off process to
 *                               be completed normally.  If the
 *                               callback function returns false
 *                               then the power-off process will
 *                               be forced to completion immediately
 *                               and this function will return.
 *                               It is advisable for the callback
 *                               function to always return true,
 *                               allowing the cellular module to
 *                               power off cleanly.
 * @return                       zero on success or negative error
 *                               code on failure.
 */
int32_t uCellPwrOff(uDeviceHandle_t cellHandle,
                    bool (*pKeepGoingCallback) (uDeviceHandle_t));

/** Remove power to the cellular module using HW lines.
 * If both the pinPwrOn and pinEnablePower parameters to
 * uCellAdd() were -1, i.e. the PWR_ON pin of the module is not
 * being controlled and there is no way to disable the power
 * supply to the module, then this function will return an error.
 *
 * @param cellHandle             the handle of the cellular instance.
 * @param trulyHard              if this is set to true and a
 *                               non-negative value for pinEnablePower
 *                               was supplied to uCellInit()
 *                               then just pull the power to the
 *                               cellular module.  ONLY USE IN
 *                               EMERGENCIES, IF THE CELLULAR MODULE
 *                               HAS BECOME COMPLETELY UNRESPONSIVE.
 *                               If a negative value for pinEnablePower
 *                               was supplied this value is treated
 *                               as false.
 * @param[in] pKeepGoingCallback even with HW lines powering the
 *                               cellular module off it is possible
 *                               for power off to take some time.
 *                               If this callback function is
 *                               non-NULL then it will be called
 *                               during the power-off process and
 *                               may be used to feed a watchdog
 *                               timer.  The callback function
 *                               should return true to allow the
 *                               power-off process to be completed
 *                               normally.  If the callback function
 *                               returns false then the power-off process
 *                               will be forced to completion immediately
 *                               and this function will return.
 *                               It is advisable for the callback
 *                               function to always return true,
 *                               allowing the cellular module to
 *                               power off cleanly. Ignored if
 *                               trulyHard is true.
 * @return                       zero on success or negative error
 *                               code on failure.
 */
int32_t uCellPwrOffHard(uDeviceHandle_t cellHandle, bool trulyHard,
                        bool (*pKeepGoingCallback) (uDeviceHandle_t));


/** If a reboot of the cellular instance is required, for example
 * due to changes that have been made to the configuration,
 * this will return true. uCellPwrReboot() should be called
 * to effect the reboot.
 *
 * @param cellHandle  the handle of the cellular instance.
 */
bool uCellPwrRebootIsRequired(uDeviceHandle_t cellHandle);

/** Re-boot the cellular module.  The module will be reset after
 * a proper detach from the network and any NV parameters will
 * be saved.  If this function returns successfully then the
 * module is ready for immediate use, no call to uCellPwrOn()
 * is required (since the SIM is not reset by a reboot).
 * TODO: is the bit about the SIM above true in all cases?
 *
 * @param cellHandle             the handle of the cellular instance.
 * @param[in] pKeepGoingCallback rebooting usually takes between 5 and
 *                               15 seconds but it is possible for it
 *                               to take longer.  If this callback
 *                               function is non-NULL then it will
 *                               be called during the re-boot
 *                               process and may be used to feed a
 *                               watchdog timer.  The callback
 *                               function should return true to
 *                               allow the re-boot process to
 *                               be completed normally.  If the
 *                               callback function returns false
 *                               then the re-boot process will
 *                               be abandoned.  Even when
 *                               this callback returns false this
 *                               function may still take some
 *                               10's of seconds to return in order
 *                               to ensure that the module is in a
 *                               cleanly powered (or not) state.
 *                               If this function is forced to return
 *                               it is advisable to call
 *                               uCellPwrIsAlive() to confirm
 *                               the final state of the module.
 * @return                       zero on success or negative error
 *                               code on failure.
 */
int32_t uCellPwrReboot(uDeviceHandle_t cellHandle,
                       bool (*pKeepGoingCallback) (uDeviceHandle_t));

/** Reset the cellular module using the given MCU pin, which should
 * be connected to the reset pin of the cellular module, for example
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
 *                   reset pin of the cellular module; if there
 *                   is an inverter between the pin of this MCU
 *                   and the pin of the module then the value
 *                   of pin should be ORed with #U_CELL_PIN_INVERTED
 *                   (defined in u_cell.h).
 * @return           zero on success or negative error
 *                   code on failure.
 */
int32_t uCellPwrResetHard(uDeviceHandle_t cellHandle, int32_t pinReset);

/** Set the DTR power-saving pin.  "UPSV" or UART power saving is
 * normally handled automatically, using activity on the UART transmit
 * data line to wake-up the module, however this is not supported on
 * LARA-R6.
 * There is also a specific case with the SARA-R5 module that needs
 * to be handled differently: when the UART flow control lines are
 * connected and UART power saving is entered the CTS line of the
 * SARA-R5 module floats high and this prevents "AT" being sent to the
 * module to wake it up again. This can be avoided by temporarily
 * suspending CTS operation through the uPortUartCtsSuspend() API but
 * there are some RTOSs (e.g. Zephyr) that do not support temporary
 * suspension of CTS.  For these cases, for SARA-R5 modules, the DTR pin
 * can be used to control UART power saving instead by calling this
 * function.
 * This must be called BEFORE the module is first powered-on, e.g.
 * just after uCellAdd() or, in the common network API, by defining
 * the structure member pinDtrPowerSaving to be the MCU pin that is
 * connected to the DTR pin of the cellular module.
 *
 * Note: the same problem exists for SARA-U201 modules and, in theory,
 * the same solution applies.  However, since we are not able to
 * regression test that configuration it is not currently marked as
 * supported in the configuration structure in u_cell_private.c.
 *
 * Note: the cellular module _remembers_ the UART power saving mode
 * and so, if you should ever change a module from DTR power saving
 * to a different UART power saving mode, you must keep the DTR pin
 * of the module asserted (i.e. tied low) in order that the AT+UPSV
 * command to change to one of the other modes can be sent.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @param pin         the pin of this MCU that is connected to
 *                    the DTR line of the cellular module; if there
 *                    is an inverter between the pin of this MCU
 *                    and the pin of the module then the value
 *                    of pin should be ORed with U_CELL_PIN_INVERTED
 *                    (defined in u_cell.h).
 * @return            zero on success or negative error
 *                    code on failure.
 */
int32_t uCellPwrSetDtrPowerSavingPin(uDeviceHandle_t cellHandle, int32_t pin);

/** Get the DTR power-saving pin.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            the pin of this MCU that is connected to
 *                    the DTR line of the cellular module, as
 *                    set by uCellPwrSetDtrPowerSavingPin(),
 *                    or negative error code.
 */
int32_t uCellPwrGetDtrPowerSavingPin(uDeviceHandle_t cellHandle);

/** Set the parameters for 3GPP power saving, only valid when in
 * Cat-M1/NB1 mode and only effective when the module is connected
 * to the cellular network.
 * If the module is registered with the network and there is no
 * radio activity (i.e. transmission to or reception from the
 * cellular network) for the duration of the active time then the
 * module will enter deep sleep.  When deep sleep is entered
 * it is as if the module has been switched off except that
 * its registration status with the cellular network is
 * preserved, it does not have to go through the registration/
 * activation process with the network on return from deep sleep.
 * HOWEVER all application-level context INSIDE the module, e.g.
 * open sockets, MQTT connections, etc., are lost: if these are
 * important to you then consider using uCellPwrSetRequestedEDrx()
 * instead.
 * The values represent a request to the network; the network may
 * apply limits to the accepted values. The current 3GPP power
 * saving parameters as agreed with the network may be read with
 * a call to uCellPwrGet3gppPowerSaving().
 * Returning the module to normal operation requires a call to
 * uCellPwrWakeUpFromDeepSleep(), which is performed AUTOMATICALLY
 * by this code when any API is called.  Note that this means it
 * is a requirement that pinPwrOn is connected to this MCU and was
 * set in the call to uCellAdd(), as that pin is used to wake the
 * module from deep sleep, and also that the VInt pin is
 * connected to this MCU and was set in the uCellAdd() call, so
 * that this code can detect when deep sleep has been entered.
 * Some modules (e.g. SARA-R4) require a re-boot for the setting
 * to be applied; it is best to check this by calling
 * uCellPwrRebootIsRequired() once this function returns.
 * 3GPP power saving is only supported when UART power saving is also
 * allowed to operate, i.e. do not define
 * U_CFG_CELL_DISABLE_UART_POWER_SAVING if you want 3GPP sleep to
 * work.
 *
 * Note: there is a corner case with SARA-R422 which is that, after
 * waking up from deep sleep, it will not re-enter deep sleep until
 * a radio connection has been made and then released.
 *
 * @param cellHandle            the handle of the cellular
 *                              instance.
 * @param rat                   the radio access technology
 *                              the setting will be applied to
 *                              for example #U_CELL_NET_RAT_CATM1 or
 *                              #U_CELL_NET_RAT_NB1 or the
 *                              return value of
 *                              uCellNetGetActiveRat() if
 *                              registered with the network.
 * @param onNotOff              true to switch 3GPP power saving
 *                              on, in which case activeTimeSeconds
 *                              and periodicWakeupSeconds must be
 *                              positive values, else false
 *                              to switch 3GPP power saving off.
 * @param activeTimeSeconds     the period of inactivity after
 *                              which the module may go to
 *                              3GPP power saving mode. The
 *                              activity time cannot be set
 *                              to less than
 *                              #U_CELL_POWER_SAVING_UART_INACTIVITY_TIMEOUT_SECONDS
 *                              in order for the wake-up code
 *                              to work.
 * @param periodicWakeupSeconds the period at which the module
 *                              wishes to wake up to inform
 *                              the cellular network that it
 *                              is still connected; this should
 *                              be set to around 1.5 times your
 *                              application's natural periodicity,
 *                              as a safety-net; the wake-up only
 *                              occurs if the module has not already
 *                              woken up for other reasons in time.
 * @return                      zero on success or negative
 *                              error code on failure.
 */
int32_t  uCellPwrSetRequested3gppPowerSaving(uDeviceHandle_t cellHandle,
                                             uCellNetRat_t rat,
                                             bool onNotOff,
                                             int32_t activeTimeSeconds,
                                             int32_t periodicWakeupSeconds);

/** Get the currently requested parameters for 3GPP power saving
 * for the current RAT.
 *
 * @param cellHandle                  the handle of the cellular
 *                                    instance.
 * @param[out] pOnNotOff              a place to put whether 3GPP power
 *                                    saving is on or off, may be NULL.
 * @param[out] pActiveTimeSeconds     a place to put the period of
 *                                    inactivity after which the module
 *                                    may go to 3GPP power saving
 *                                    mode; may be NULL.
 * @param[out] pPeriodicWakeupSeconds a place to put the period at
 *                                    which the module wishes to
 *                                    wake-up to inform the cellular
 *                                    network that it is still
 *                                    connected; may be NULL.
 * @return                            zero on success or negative
 *                                    error code on failure.
 */
int32_t uCellPwrGetRequested3gppPowerSaving(uDeviceHandle_t cellHandle,
                                            bool *pOnNotOff,
                                            int32_t *pActiveTimeSeconds,
                                            int32_t *pPeriodicWakeupSeconds);

/** Get the 3GPP power saving parameters as agreed with the cellular
 * network for the current RAT.
 *
 * @param cellHandle                  the handle of the cellular
 *                                    instance.
 * @param[out] pOnNotOff              a place to put whether 3GPP power
 *                                    saving is on or off, may be NULL.
 * @param[out] pActiveTimeSeconds     a place to put the period of
 *                                    inactivity after which the module
 *                                    may go to 3GPP power saving
 *                                    mode; may be NULL.
 * @param[out] pPeriodicWakeupSeconds a place to put the period at
 *                                    which the module wishes to
 *                                    wake-up to inform the cellular
 *                                    network that it is still
 *                                    connected; may be NULL.
 * @return                            zero on success or negative
 *                                    error code on failure.
 */
int32_t uCellPwrGet3gppPowerSaving(uDeviceHandle_t cellHandle,
                                   bool *pOnNotOff,
                                   int32_t *pActiveTimeSeconds,
                                   int32_t *pPeriodicWakeupSeconds);

/** Set a callback which will be called when the assigned 3GPP
 * power saving parameters are changed by the network, either when
 * they are first set up or on a cell/tracking area change.
 * The callback is implemented using the uAtClientCallback() queue,
 * see the AT client API for details. The callback should not block;
 * use the callback to signal something else to do any heavy-lifting
 * and then return, otherwise important operations such as reacting
 * to URCs sent by the module will be adversely affected.
 *
 * @param cellHandle          the handle of the cellular instance.
 * @param[in] pCallback       a callback which will be called when
 *                            the assigned 3GPP power saving parameters
 *                            are changed by the network; the first
 *                            parameter will be the cellular handle,
 *                            the second indicates whether 3GPP power saving
 *                            is enabled or not, the third will be the
 *                            assigned active time in seconds, the
 *                            fourth the assigned periodic wake-up time
 *                            in seconds and the fifth will be
 *                            pCallbackParam. Use NULL to remove a
 *                            previous callback.
 * @param[in] pCallbackParam  a parameter that will be passed
 *                            to pCallback as its last parameter
 *                            when it is called; may be NULL.
 * @return                    zero on success or negative error
 *                            code on failure.
 */
int32_t uCellPwrSet3gppPowerSavingCallback(uDeviceHandle_t cellHandle,
                                           void (*pCallback) (uDeviceHandle_t cellHandle,
                                                              bool onNotOff,
                                                              int32_t activeTimeSeconds,
                                                              int32_t periodicWakeupSeconds,
                                                              void *pCallbackParam),
                                           void *pCallbackParam);

/** Get the current state of 3GPP power saving.
 *
 * IMPORTANT: as explained in the comments against
 * #uCellPwr3gppPowerSavingState_t and in the detailed description at
 * the top of this file, 3GPP power saving and the sleep-state of the
 * cellular module are _different_ things: 3GPP power saving can
 * be active and the module can still be fully awake and consuming lots
 * of power; please do not confuse the two.
 *
 * @param cellHandle        the handle of the cellular instance.
 * @param[out] pApplication if the 3GPP power saving state is
 *                          #U_CELL_PWR_3GPP_POWER_SAVING_STATE_BLOCKED_BY_MODULE
 *                          then, if this parameter is non-NULL, it will be
 *                          populated with the number of the application inside
 *                          the module that is blocking entry to deep sleep; this
 *                          number is module-specific, please refer to the power
 *                          management section of the AT manual for your module
 *                          for further information.
 * @return                  the 3GPP power saving state.
 */
uCellPwr3gppPowerSavingState_t uCellPwrGet3gppPowerSavingState(uDeviceHandle_t cellHandle,
                                                               int32_t *pApplication);

/** Set the requested E-DRX parameters.  E-DRX is only effective
 * when the module is connected to the cellular network.  When
 * E-DRX is activated then, when the module returns to idle after
 * a radio transmission, it will listen for downlink messages for
 * an additional pagingWindowSeconds and then it will be allowed to
 * enter a low power state but not the deep sleep state of 3GPP
 * power saving and hence the module internal state (sockets, MQTT
 * connectivity, etc.) is preserved; this power saving behaviour
 * is more suitable when an application is using the sockets, MQTT,
 * location etc. APIs of ubxlib.  After eDrxSeconds have passed the
 * module will wake up for pagingWindowSeconds again to listen for
 * downlink messages from the network, then the eDrxSeconds timer
 * will start again, etc.  That module will wake up to send any
 * uplink messages that are required, they are unaffected, and any
 * responses to those messages arriving within a few seconds,
 * before the module returns to idle, will also arrive immediately,
 * it is the latency of _occasional_ downlink communication that
 * changes with the E-DRX period; you should set eDrxSeconds to
 * less than any minimum downlink latency that your application
 * might require (if any).
 * The values represent a request to the network; the network
 * may apply limits to the accepted values.  The current E-DRX
 * parameters as agreed with the network may be read with a call to
 * uCellPwrGetEDrx().  Some modules, e.g. SARA-R4, will ONLY
 * allow the E-DRX values to be set when the module is NOT registered
 * with the network, hence it is necessary to pass the RAT that
 * will be used into this function call as the coding of the E-DRX
 * values transmitted to the network are RAT dependent and this
 * code cannot discover the current RAT when not registered.
 * If you are using a module type which supports setting the E-DRX
 * parameters while connected to the network (e.g. SARA-R5) then
 * you may pass the return value of uCellNetGetActiveRat() as
 * the RAT.  Some module types (e.g. SARA-R4) must be re-booted
 * for the settings to be applied; please check if this is the
 * case with a call to uCellPwrRebootIsRequired() after calling
 * this function.
 * E-DRX is only supported by this code when UART power saving is
 * also allowed to operate, i.e. do not define
 * U_CFG_CELL_DISABLE_UART_POWER_SAVING if you want E-DRX to
 * work.
 *
 * Note: there is a corner case if both 3GPP power saving and E-DRX
 * are applied, which is that if the module enters deep sleep
 * as a result of 3GPP power saving and then is awoken to do
 * something that does _not_ cause radio activity (e.g. read from
 * a GNSS module that is attached to the cellular module, read
 * from the cellular file system, etc.) then the module will NOT
 * re-enter E-DRX immediately. This is because E-DRX is only entered
 * after *leaving* connected state and wake-up from deep sleep after
 * 3GPP power saving is specifically designed not to send any radio
 * transmission to the network in order to save power, hence it
 * does not enter, and so does not leave connected state.  Only
 * after a radio transmission is sent will E-DRX be entered once
 * more.
 *
 * @param cellHandle            the handle of the cellular
 *                              instance.
 * @param rat                   the radio access technology
 *                              the setting will be applied to
 *                              for example #U_CELL_NET_RAT_CATM1 or
 *                              #U_CELL_NET_RAT_NB1 or the
 *                              return value of
 *                              uCellNetGetActiveRat() if
 *                              registered with the network.
 * @param onNotOff              true to switch E-DRX on, in
 *                              which case eDrxSeconds and
 *                              pagingWindowSeconds must be
 *                              positive values, else false
 *                              to switch E-DRX off.
 * @param eDrxSeconds           the E-DRX value in seconds.
 * @param pagingWindowSeconds   the period of inactivity after
 *                              which the module should go to
 *                              sleep.
 *                              IMPORTANT: not all platforms
 *                              support this parameter, it is
 *                              ignored where this is the case.
 * @return                      zero on success or negative
 *                              error code on failure.
 */
int32_t uCellPwrSetRequestedEDrx(uDeviceHandle_t cellHandle,
                                 uCellNetRat_t rat,
                                 bool onNotOff,
                                 int32_t eDrxSeconds,
                                 int32_t pagingWindowSeconds);

/** Get the requested E-DRX parameters for the given RAT.
 *
 * @param cellHandle                  the handle of the cellular
 *                                    instance.
 * @param rat                         the radio access technology
 *                                    for example #U_CELL_NET_RAT_CATM1 or
 *                                    #U_CELL_NET_RAT_NB1 or the
 *                                    return value of
 *                                    uCellNetGetActiveRat() if
 *                                    registered with the network.
 * @param[out] pOnNotOff              a place to put whether E-DRX
 *                                    has been requested to be on or
 *                                    off, may be NULL.
 * @param[out] pEDrxSeconds           a place to put the requested
 *                                    E-DRX value in seconds; may be
 *                                    NULL.
 * @param[out] pPagingWindowSeconds   a place to put the requested
 *                                    paging window value in seconds;
 *                                    may be NULL. IMPORTANT: not all
 *                                    platforms support reading this
 *                                    parameter, even if they support
 *                                    setting it, in which case -1 will
 *                                    be returned for this value.
 * @return                            zero on success or negative
 *                                    error code on failure.
 */
int32_t uCellPwrGetRequestedEDrx(uDeviceHandle_t cellHandle,
                                 uCellNetRat_t rat,
                                 bool *pOnNotOff,
                                 int32_t *pEDrxSeconds,
                                 int32_t *pPagingWindowSeconds);

/** Get the E-DRX parameters as agreed with the cellular network
 * for the given RAT.  The module must be connected to the cellular
 * network for this to work.
 *
 * @param cellHandle                  the handle of the cellular
 *                                    instance.
 * @param rat                         the radio access technology
 *                                    for example #U_CELL_NET_RAT_CATM1 or
 *                                    #U_CELL_NET_RAT_NB1 or the
 *                                    return value of
 *                                    uCellNetGetActiveRat() if
 *                                    registered with the network.
 * @param[out] pOnNotOff              a place to put whether E-DRX
 *                                    is on or off, may be NULL.
 * @param[out] pEDrxSeconds           a place to put the E-DRX value
 *                                    in seconds; may be NULL.
 * @param[out] pPagingWindowSeconds   a place to put the paging window
 *                                    vaue in seconds; may be NULL.
 * @return                            zero on success or negative error
 *                                    code on failure.
 */
int32_t uCellPwrGetEDrx(uDeviceHandle_t cellHandle,
                        uCellNetRat_t rat,
                        bool *pOnNotOff,
                        int32_t *pEDrxSeconds,
                        int32_t *pPagingWindowSeconds);

/** Set a callback which will be called when the E-DRX parameters
 * change.  After setting the requested E-DRX parameters with a
 * call to uCellPwrSetRequestedEDrx(), the parameters (even the
 * requested values) may not be changed by the module immediately,
 * and they may be changed at any time by the network.  Use this
 * callback to find out when new values are assigned.
 * The callback is implemented using the uAtClientCallback() queue,
 * see the AT client API for details. The callback should not block;
 * use the callback to signal something else to do any heavy-lifting
 * and then return, otherwise important operations such as reacting
 * to URCs sent by the module will be adversely affected.
 *
 * @param cellHandle          the handle of the cellular instance.
 * @param[in] pCallback       a callback which will be called when
 *                            the E-DRX parameters change; the first
 *                            parameter will be the cellular handle,
 *                            the second the RAT to which the E-DRX
 *                            parameters apply, the third whether
 *                            E-DRX is on or off for that RAT, the
 *                            fourth the requested E-DRX value in
 *                            seconds, the fifth the assigned E-DRX value
 *                            in seconds, the sixth the assigned
 *                            paging window value in seconds and the
 *                            seventh pCallbackParam. Use NULL to
 *                            remove a previous callback.
 * @param[in] pCallbackParam  a parameter that will be passed
 *                            to pCallback as its last parameter
 *                            when it is called; may be NULL.
 * @return                    zero on success or negative error
 *                            code on failure.
 */
int32_t uCellPwrSetEDrxCallback(uDeviceHandle_t cellHandle,
                                void (*pCallback) (uDeviceHandle_t cellHandle,
                                                   uCellNetRat_t rat,
                                                   bool onNotOff,
                                                   int32_t eDrxSecondsRequested,
                                                   int32_t eDrxSecondsAssigned,
                                                   int32_t pagingWindowSecondsAssigned,
                                                   void *pCallbackParam),
                                void *pCallbackParam);

/** Set callback for wake-up from deep sleep.  The callback will be
 * called when the module has returned from deep sleep and may be used
 * to set back up any configuration that would have been lost due to
 * the module being effectively off.  Only modules that have their VInt
 * pin connected to this MCU and that pin was set in the uCellAdd()
 * call are able to support this indication.  The callback is implemented
 * using the uAtClientCallback() queue, see the AT client API for details.
 * The callback should not block; use the callback to signal something
 * else to do the heavy-lifting and then return, otherwise important
 * operations such as reacting to URCs sent by the module will be
 * adversely affected.
 *
 * @param cellHandle           the handle of the cellular instance.
 * @param[in] pCallback        a callback which will be called when
 *                             the module leaves deep sleep; use
 *                             NULL to remove a previous wake-up
 *                             callback; the first parameter to the
 *                             callback will be the cellular Handle,
 *                             the second will be pCallbackParam.
 * @param[in] pCallbackParam   a parameter that will be passed
 *                             to pCallbackParam as its second
 *                             parameter when it is called; may be
 *                             NULL.
 * @return                     zero on success or negative error
 *                             code on failure.
 */
int32_t uCellPwrSetDeepSleepWakeUpCallback(uDeviceHandle_t cellHandle,
                                           void (*pCallback) (uDeviceHandle_t cellHandle,
                                                              void *pCallbackParam),
                                           void *pCallbackParam);

/** Get whether deep sleep is currently active or not: if the module's
 * VInt pin is connected to a pin of this MCU and that pin was set
 * in the uCellAdd() then pSleepActive will be set to true if the
 * module is actually in deep sleep.
 *
 * @param cellHandle        the handle of the cellular instance.
 * @param[out] pSleepActive a place to put whether deep sleep is active
 *                          or not (true IF SLEEP IS ACTIVE, the
 *                          module is effectively off, else false); may
 *                          be NULL (for example if you just want to find out
 *                          if the callback is supported).
 * @return                  zero on success else negative error code
 *                          if the module does not support indicating
 *                          its sleep state.
 */
int32_t uCellPwrGetDeepSleepActive(uDeviceHandle_t cellHandle,
                                   bool *pSleepActive);

/** Wake the module from deep sleep.  THERE SHOULD BE NO NEED
 * FOR THE USER TO CALL THIS; it will be called automatically by
 * the AT client if it needs to do something after the module
 * has entered deep sleep.
 *
 * @param cellHandle             the handle of the cellular instance.
 * @param[in] pKeepGoingCallback waking from deep sleep usually takes
 *                               between 5 and 15 seconds but it is
 *                               possible for it to take longer.  If
 *                               this callback function is non-NULL
 *                               then it will be called during the
 *                               wake-up process and may be used to
 *                               feed a watchdog timer.  The callback
 *                               function should return true to
 *                               allow the wake-up process to
 *                               be completed normally.  If the
 *                               callback function returns false
 *                               then the wake-up process will
 *                               be abandoned.  Even when
 *                               this callback returns false it
 *                               may still take some 10's of
 *                               seconds to return in order to
 *                               ensure that the module is in a
 *                               cleanly powered (or not) state.
 *                               If this function is forced to return
 *                               it is advisable to call
 *                               uCellPwrIsAlive() to confirm
 *                               the final state of the module.
 * @return                       zero on success or negative error
 *                               code on failure.
 */
int32_t uCellPwrWakeUpFromDeepSleep(uDeviceHandle_t cellHandle,
                                    bool (*pKeepGoingCallback) (uDeviceHandle_t));

/** Disable UART, AKA 32 kHz, sleep.
 * 32 kHz sleep is always enabled where supported by the module;
 * call this function to disable 32 kHz sleep.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            zero on success or negative error code on
 *                    failure.
 */
int32_t uCellPwrDisableUartSleep(uDeviceHandle_t cellHandle);

/** Enable UART, AKA 32 kHz sleep.  32 kHz sleep is always enabled
 * where supported - you only need to call this if you have
 * previously called uCellPwrDisableUartSleep().
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            zero on success or negative error code on
 *                    failure.
 */
int32_t uCellPwrEnableUartSleep(uDeviceHandle_t cellHandle);

/** Determine whether UART, AKA 32 kHz, sleep is enabled or not.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            true if UART sleep is enabled, else false.
 */
bool uCellPwrUartSleepIsEnabled(uDeviceHandle_t cellHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_CELL_PWR_H_

// End of file
