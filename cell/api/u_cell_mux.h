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

#ifndef _U_CELL_MUX_H_
#define _U_CELL_MUX_H_

/* Only header files representing a direct and unavoidable
 * dependency between the API of this module and the API
 * of another module should be included here; otherwise
 * please keep #includes to your .c files. */

#include "u_device.h"

/** \addtogroup _cell _Cellular
 *  @{
 */

/** @file
 * @brief This header file defines the cellular APIs that initiate
 * 3GPP 27.010 CMUX operation.
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

/** The channel ID to use for access to a GNSS chip embedded inside
 * or attached via a cellular module; where access to a GNSS device
 * over CMUX is supported (so not LENA-R8) this will be translated into
 * the correct channel number for the cellular module in use.
 */
#define U_CELL_MUX_CHANNEL_ID_GNSS 0xFF

#ifndef U_CELL_MUX_MAX_CHANNELS
/** Enough room for the control channel, an AT channel, a
 * GNSS serial channel and potentially a PPP data channel.
 */
# define U_CELL_MUX_MAX_CHANNELS 4
#endif

/* ----------------------------------------------------------------
 * FUNCTIONS:  WORKAROUND FOR LINKER ISSUE
 * -------------------------------------------------------------- */

/** Workaround for Espressif linker missing out files that
 * only contain functions which also have weak alternatives
 * (see https://www.esp32.com/viewtopic.php?f=13&t=8418&p=35899).
 *
 * You can ignore this function.
 */
void uCellMuxPrivateLink(void);

/* ----------------------------------------------------------------
 * FUNCTIONS
 * -------------------------------------------------------------- */

/** Enable multiplexer mode.  Puts the cellular module's AT
 * interface into multiplexer (3GPP 27.010 CMUX) mode.  This
 * is useful when you want to access a GNSS module that is
 * connected via, or embedded inside, a cellular module as if it
 * were connected directly to this MCU via a serial interface (see
 * uCellMuxAddChannel()).  Note that this function _internally_
 * opens and uses a CMUX channel for the AT interface, you do not
 * have to do that.  The AT handle that was originally passed to
 * uCellAdd() will remain locked, the handle of the new one that is
 * created for use internally can be obtained by calling
 * uCellAtClientHandleGet(); uCellAtClientHandleGet() will always
 * return the AT handle currently in use.
 *
 * Whether multiplexer mode is supported or not depends on the cellular
 * module and the interface in use: for instance a USB interface to
 * a module does not support multiplexer mode.
 *
 * The module must be powered on for this to work.  Returns success
 * without doing anything if multiplexer mode is already enabled.
 * Multiplexer mode does not survive a power-cycle, either deliberate
 * (with uCellPwrOff(), uCellPwrReboot(), etc.) or accidental, and
 * cannot be used with 3GPP power saving (since it will also be
 * reset during module deep sleep).
 *
 * Note: if you have passed the AT handle to a GNSS instance (e.g.
 * via uGnssAdd()) it will stop working when multiplexer mode is
 * enabled (because the AT handle will have been changed), hence you
 * should enable multiplexer mode _before_ calling uGnssAdd()
 * (and, likewise, remove any GNSS instance before disabling
 * multiplexer mode).  However, if you have enabled multiplexer
 * mode on a device where GNSS can be accessed via CMUX (i.e. NOT
 * LENA-R8) then it is much better to call uCellMuxAddChannel() with
 * #U_CELL_MUX_CHANNEL_ID_GNSS and then you can pass the
 * #uDeviceSerial_t handle that returns to uGnssAdd() (with the
 * transport type #U_GNSS_TRANSPORT_VIRTUAL_SERIAL) and you will
 * have streamed position.
 *
 * @param cellHandle the handle of the cellular instance.
 * @return           zero on success or negative error code on failure.
 */
int32_t uCellMuxEnable(uDeviceHandle_t cellHandle);

/** Determine if the multiplexer is currently enabled.
 *
 * @param cellHandle the handle of the cellular instance.
 * @return           true if the multiplexer is enabled, else false.
 */
bool uCellMuxIsEnabled(uDeviceHandle_t cellHandle);

/** Add a multiplexer channel; may be called after uCellMuxEnable()
 * has returned success in order to, for instance, create a virtual
 * serial port to a GNSS chip inside a SARA-R422M8S or SARA-R510M8S
 * module (but not a LENA-R8001M10 module, where access to the built-in
 * GNSS device over CMUX is not supported).  The virtual serial port
 * handle returned in *ppDeviceSerial can be used in #uDeviceCfg_t to
 * open the GNSS device using the uDevice API, or it can be passed to
 * uGnssAdd() (with the transport type #U_GNSS_TRANSPORT_VIRTUAL_SERIAL)
 * if you prefer to use the uGnss API the hard way.
 *
 * If the channel is already open, this function returns success
 * without doing anything.  An error is returned if uCellMuxEnable()
 * has not been called.
 *
 * Note: there is a known issue with SARA-R5 modules where, if a GNSS
 * multiplexer channel is opened, closed, and then re-opened the GNSS
 * chip will be unresponsive.  For that case, please open the GNSS
 * multiplexer channel once at start of day.
 *
 * UART POWER SAVING: when UART power saving is enabled in the module
 * any constraints arising will also apply to a multiplexer channel;
 * specifically, if a DTR pin is not used to wake-up the module, i.e.
 * the module supports and is using the "wake up on TX activity" mode
 * of UART power saving then, though the AT interface will continue
 * to work correctly (as it knows to expect loss of the first few
 * characters of an AT string), the other multiplexer channels have
 * the same restriction and have no such automated protection. Hence
 * if you (a) expect to use a multiplexer channel to communicate with
 * a GNSS chip in a cellular module and (b) are not able to use a DTR
 * pin to wake the module up from power-saving, then you should call
 * uCellPwrDisableUartSleep() to disable UART sleep while you run the
 * multiplexer channel (and uCellPwrEnableUartSleep() to re-enable it
 * afterwards).
 *
 * NOTES ON DEVICE SERIAL OPERATION: the operation of *pDeviceSerial
 * is constrained in certain ways, since what you have is not a real
 * serial port, it is a virtual serial port which has hijacked some
 * of the functionality of the physical serial port that was
 * previously running, see notes below, but particularly flow control,
 * or not taking data out of one or more multiplexed serial ports fast
 * enough, can have an adverse effect on other multiplexed serial ports.
 * This is difficult to avoid since they are on the same transport.  Hence
 * it is important to service your multiplexed serial ports often or,
 * alternatively, you may call serialDiscardOnFlowControl() with true
 * on any serial port where you are happy for any overruns to be
 * discarded (e.g. the GNSS one), so that it cannot possibly interfere
 * with others (e.g. the AT command one).
 *
 * The stack size and priority of any event serial callbacks are not
 * respected: what you end up with is #U_CELL_MUX_CALLBACK_TASK_PRIORITY
 * and #U_CELL_MUX_CALLBACK_TASK_STACK_SIZE_BYTES since a common
 * event queue is used for all serial devices.
 *
 * @param cellHandle          the handle of the cellular instance.
 * @param channel             the channel number to open; channel
 *                            numbers are module-specific, however
 *                            the value #U_CELL_MUX_CHANNEL_ID_GNSS
 *                            can be used, in all cases except LENA-R8
 *                            (which does not support access to GNSS
 *                            over CMUX), to open a channel to an
 *                            embedded GNSS chip.  Note that channel
 *                            zero is reserved for management operations
 *                            and channel one is the existing AT interface;
 *                            neither value can be used here.
 * @param[out] ppDeviceSerial a pointer to a place to put the
 *                            handle of the virtual serial port
 *                            that is the multiplexer channel.
 * @return                    zero on success or negative error
 *                            code on failure.
 */
int32_t uCellMuxAddChannel(uDeviceHandle_t cellHandle,
                           int32_t channel,
                           uDeviceSerial_t **ppDeviceSerial);

/** Get the serial device for an open multiplexer channel.
 *
 * @param cellHandle the handle of the cellular instance.
 * @param channel    the channel number.
 * @return           the serial device for the channel,
 *                   else NULL if the channel is not open.
 */
uDeviceSerial_t *pUCellMuxChannelGetDeviceSerial(uDeviceHandle_t cellHandle,
                                                 int32_t channel);

/** Remove a multiplexer channel.  Note that this does NOT free
 * memory to ensure thread safety; memory is free'd when the cellular
 * instance is closed (or see uCellMuxFree()).
 *
 * @param cellHandle        the handle of the cellular instance.
 * @param[in] pDeviceSerial the handle of the virtual serial port that
 *                          is the multiplexer channel, as returned
 *                          by uCellMuxAddChannel().
 * @return                  zero on success or negative error code on
 *                          failure.
 */
int32_t uCellMuxRemoveChannel(uDeviceHandle_t cellHandle,
                              uDeviceSerial_t *pDeviceSerial);

/** Disable multiplexer mode.  Any currently active multiplexer channels
 * will be deactivated first.  Returns success without doing anything if
 * uCellMuxEnable() has not been called.  Note that this does NOT free
 * memory to ensure thread safety; memory is free'd when the cellular
 * instance is closed.  When this function has returned successfully
 * the internal AT handler that was created for multiplexer mode will
 * no longer be in use and the AT handle will return to being the one
 * originally passed to uCellAdd(); uCellAtClientHandleGet() will reflect
 * this change.
 *
 * IMPORTANT: if you have compiled with U_CFG_ENABLE_PPP, in order to
 * use the native OS IP stack with a cellular connection, you should
 * NOT call this function; it would result in the PPP connection, which
 * uses the multiplexer, being terminated without notice and you will
 * find that any subsequent attempt to make a PPP connection to the
 * module will fail (since the previous one is still up), until you
 * have power-cycled or rebooted the module.
 *
 * @param cellHandle the handle of the cellular instance.
 * @return           zero on success or negative error code on failure.
 */
int32_t uCellMuxDisable(uDeviceHandle_t cellHandle);

/** uCellMuxRemoveChannel() / uCellMuxDisable() do not free memory in
 * order to ensure thread-safety: should any asynchronous callback functions,
 * for example carrying user data, occur as a multiplexer is being
 * closed they might otherwise call into free()'ed memory space; memory
 * is only free()'ed when the cellular instance is closed.  However,
 * if you can't wait, you really need that memory back, and you are
 * absolutely sure that there is no chance of an asynchronous event
 * occurring, you may call this function to regain heap.  Note that
 * this only does the memory-freeing part, not the closing down
 * part, i.e. you must have called uCellMuxRemoveChannel(), or called
 * uCellMuxDisable(), for it to have any effect.
 *
 * @param cellHandle  the handle of the cellular instance.
 */
void uCellMuxFree(uDeviceHandle_t cellHandle);

/** Abort multiplexer mode in the module.  You do NOT normally need
 * to use this function, it does nothing to the state of the multiplexer
 * as far as this MCU is concerned, it doesn't close anything or
 * free memory or tidy anything up or do anything at all, etc., all it
 * does is send a "magic sequence" to the module which the module should
 * interpret as "leave multiplexer mode".
 *
 * This may be useful if, somehow, this code has got out of sync with
 * the module, so this code is not in multiplexer mode but the module
 * is.  In this situation the module may appear unresponsive, since
 * its multiplexer output will make no sense to the AT client.  Of
 * course, calling uCellPwrOffHard() or uCellPwrResetHard() is likely
 * a better approach but if you cannot use either of those functions
 * for any reason (e.g. you do not have HW lines connected from this
 * MCU to the module's PWR_ON or reset pins) then you may try this
 * approach to return the module to responsiveness.
 *
 * @param cellHandle  the handle of the cellular instance.
 * @return            zero if the magic sequence has been successfully
 *                    sent to the module, negative error code on failure.
 *                    Note that success does not mean that the module
 *                    is back in normal mode, just that the sequence
 *                    has been sent; you may determine if the module
 *                    is now in normal mode by calling uCellPwrIsAlive()
 *                    or whatever.
 */
int32_t uCellMuxModuleAbort(uDeviceHandle_t cellHandle);

#ifdef __cplusplus
}
#endif

/** @}*/

#endif // _U_CELL_MUX_H_

// End of file
